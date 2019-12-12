// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.IO;
using Unity.Collections;
using Unity.Collections.LowLevel.Unsafe;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.XR;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// Custom video source capturing the Unity scene content as rendered by a given camera,
    /// and sending it as a video track through the selected peer connection.
    /// </summary>
    public class SceneVideoSource : CustomVideoSource<Argb32VideoFrameStorage>
    {
        internal static readonly int s_InputTexID = Shader.PropertyToID("_InputTex");

        /// <summary>
        /// Camera used to capture the scene content, whose rendering is used as
        /// video content for the track.
        /// </summary>
        /// <remarks>
        /// If the project uses Multi-Pass stereoscopic rendering, then this camera needs to
        /// render to a single eye to produce a single video frame. Generally this means that
        /// this needs to be a separate Unity camera from the one used for XR rendering, which
        /// is generally rendering to both eyes.
        /// 
        /// If the project uses Single-Pass Instanced stereoscopic rendering, then Unity 2019.1+
        /// is required to make this component work, due to the fact earlier versions of Unity
        /// are missing some command buffer API calls to be able to efficiently access the camera
        /// backbuffer in this mode. For Unity 2018.3 users who cannot upgrade, use Single-Pass
        /// (non-instanced) instead.
        /// </remarks>
        [Header("Camera")]
        [Tooltip("Camera used to capture the scene content sent by the track.")]
        [CaptureCamera]
        public Camera SourceCamera;

        /// <summary>
        /// Camera event indicating the point in time during the Unity frame rendering
        /// when the camera rendering is to be captured.
        /// 
        /// This defaults to <see xref="CameraEvent.AfterEverything"/>, which is a reasonable
        /// default to capture the entire scene rendering, but can be customized to achieve
        /// other effects like capturing only a part of the scene.
        /// </summary>
        [Tooltip("Camera event when to insert the scene capture at.")]
        public CameraEvent CameraEvent = CameraEvent.AfterEverything;

        /// <summary>
        /// Match the video frame size produced for the local WebRTC track with the size of the
        /// Unity camera back-buffer, avoiding any upscale/downscale during capture.
        /// </summary>
        [Tooltip("Use camera backbuffer size for video track frame size.")]
        public bool UseCameraSize = true;

        /// <summary>
        /// Size, in pixels, of the frame produced for the video track. This can be different from
        /// the size of the camera backbuffer, in which case the GPU will resize (upscale/downscale)
        /// the content of the camera backbuffer before it is read back to CPU memory for dispatching
        /// to the local video track.
        /// 
        /// This is only taken into account if <see cref="UseCameraSize"/> is <c>false</c>.
        /// </summary>
        [Tooltip("If UseCameraSize is false, the video track frame size to use.")]
        public Vector2Int CustomFrameSize = Vector2Int.zero;

        /// <summary>
        /// Command buffer attached to the camera to capture its rendered content from the GPU
        /// and transfer it to the CPU for dispatching to WebRTC.
        /// </summary>
        private CommandBuffer _commandBuffer;

        /// <summary>
        /// Read-back texture where the content of the camera backbuffer is copied before being
        /// transferred from GPU to CPU. The size of the texture is <see cref="_frameSize"/>.
        /// </summary>
        private RenderTexture _readBackTex;

        /// <summary>
        /// Cached size in pixels of the frame size, either equal to <see cref="CustomFrameSize"/> or
        /// to the actual Unity camera back-buffer size depending on the value of <see cref="UseCameraSize"/>.
        /// </summary>
        private Vector2Int _frameSize = Vector2Int.zero;

        private Material _scaleMaterial = null;
        private Texture2D _convertTex = null;

        protected new void OnEnable()
        {
            if (SourceCamera == null)
            {
                throw new NullReferenceException("Empty source camera for SceneVideoSource.");
            }

            CreateCommandBuffer();
            SourceCamera.AddCommandBuffer(CameraEvent, _commandBuffer);

            // Enable the track
            base.OnEnable();
        }

        protected new void OnDisable()
        {
            // Disable the track
            base.OnDisable();

            // The camera sometimes goes away before this component, in which case
            // it already removed the command buffer from itself anyway.
            if (SourceCamera != null)
            {
                SourceCamera.RemoveCommandBuffer(CameraEvent, _commandBuffer);
            }
            _commandBuffer.Dispose();
            _commandBuffer = null;
        }

        /// <summary>
        /// Create the command buffer reading the scene content from the source camera back into CPU memory
        /// and delivering it via the <see cref="OnSceneFrameReady(AsyncGPUReadbackRequest)"/> callback to
        /// the underlying WebRTC track.
        /// </summary>
        private void CreateCommandBuffer()
        {
            if (_commandBuffer != null)
            {
                throw new InvalidOperationException("Command buffer already initialized.");
            }

            // Handle stereoscopic rendering for VR/AR.
            // See https://unity3d.com/how-to/XR-graphics-development-tips for details.
            var sourceSize = new Vector2Int(SourceCamera.scaledPixelWidth, SourceCamera.scaledPixelHeight);
            RenderTextureFormat srcFormat = RenderTextureFormat.ARGB32;
            bool needHalfWidth = false;
            if (SourceCamera.stereoEnabled)
            {
                // The readback will occur on the left eye (chosen arbitrarily).

                // Readback size is the size of the texture for a single eye.
                sourceSize = new Vector2Int(XRSettings.eyeTextureWidth, XRSettings.eyeTextureHeight);
                srcFormat = XRSettings.eyeTextureDesc.colorFormat;

                if (XRSettings.stereoRenderingMode == XRSettings.StereoRenderingMode.MultiPass)
                {
                    // Multi-pass is similar to non-stereo, nothing to do.

                    // Ensure camera is not rendering to both eyes in multi-pass stereo, otherwise the command buffer
                    // is executed twice (once per eye) and will produce twice as many frames, which leads to stuttering
                    // when playing back the video stream resulting from combining those frames.
                    if (SourceCamera.stereoTargetEye == StereoTargetEyeMask.Both)
                    {
                        throw new InvalidOperationException("SourceCamera has stereoscopic rendering enabled to both eyes" +
                            " with multi-pass rendering (XRSettings.stereoRenderingMode = MultiPass). This is not supported" +
                            " with SceneVideoSource, as this would produce one image per eye. Either set XRSettings." +
                            "stereoRenderingMode to single-pass (instanced or not), or use multi-pass with a camera rendering" +
                            " to a single eye (Camera.stereoTargetEye != Both).");
                    }
                }
                else if (XRSettings.stereoRenderingMode == XRSettings.StereoRenderingMode.SinglePass)
                {
                    // Single-pass (non-instanced) stereo use "wide-buffer" packing.
                    // Left eye corresponds to the left half of the buffer.
                    needHalfWidth = true;
                }
                else if ((XRSettings.stereoRenderingMode == XRSettings.StereoRenderingMode.SinglePassInstanced)
                    || (XRSettings.stereoRenderingMode == XRSettings.StereoRenderingMode.SinglePassMultiview)) // same as instanced (OpenGL)
                {
                    // Single-pass instanced stereo use texture array packing.
                    // Left eye corresponds to the first array slice.
#if !UNITY_2019_1_OR_NEWER
                    // https://unity3d.com/unity/alpha/2019.1.0a13
                    // "Graphics: Graphics.Blit and CommandBuffer.Blit methods now support blitting to and from texture arrays."
                    throw new NotSupportedException("Capturing scene content in single-pass instanced stereo rendering requires" +
                        " blitting from the Texture2DArray render target of the camera, which is not supported before Unity 2019.1." +
                        " To use this feature, either upgrade your project to Unity 2019.1+ or use single-pass non-instanced stereo" +
                        " rendering (XRSettings.stereoRenderingMode = SinglePass).");
#endif
                }
            }

            bool needResize = false;
            if (UseCameraSize)
            {
                _frameSize = sourceSize;
            }
            else
            {
                if ((CustomFrameSize.x <= 0) || (CustomFrameSize.y <= 0))
                {
                    throw new ArgumentOutOfRangeException("CustomFrameSize", CustomFrameSize, "Invalid custom frame size.");
                }
                _frameSize = CustomFrameSize;
                needResize = (sourceSize != CustomFrameSize);
            }

            _readBackTex = new RenderTexture(_frameSize.x, _frameSize.y, 0, srcFormat, RenderTextureReadWrite.Linear);

            _commandBuffer = new CommandBuffer
            {
                name = "SceneVideoSource"
            };

            // Explicitly set the render target to instruct the GPU to discard previous content.
            // https://docs.unity3d.com/ScriptReference/Rendering.CommandBuffer.Blit.html recommends this.
            //< TODO - This doesn't work
            //_commandBuffer.SetRenderTarget(_readBackTex, RenderBufferLoadAction.DontCare, RenderBufferStoreAction.Store);

            // Copy camera backbuffer to readback texture
            _commandBuffer.BeginSample("Blit");
            if (needResize)
            {
                int convertWidth = sourceSize.x;
                var srcOffset = Vector2.zero;
                var srcScale = Vector2.one;
                if (needHalfWidth)
                {
                    //convertWidth *= 2;
                    srcScale.x = 0.5f;
                }
                var rtId = Shader.PropertyToID("tempRT");
                _commandBuffer.GetTemporaryRT(rtId, sourceSize.x, sourceSize.y, 0, FilterMode.Point, RenderTextureFormat.ARGB32);
#if UNITY_2019_1_OR_NEWER
                int srcSliceIndex = 0; // left eye
                int dstSliceIndex = 0;
                _commandBuffer.Blit(BuiltinRenderTextureType.CameraTarget, _readBackTex, srcScale, srcOffset, srcSliceIndex, dstSliceIndex);
#else
                _commandBuffer.Blit(BuiltinRenderTextureType.CameraTarget, rtId, srcScale, srcOffset);
#endif
                _convertTex = new Texture2D(convertWidth, sourceSize.y, TextureFormat.RGBA32, mipChain: false);
                _commandBuffer.ConvertTexture(rtId, _convertTex);
                _commandBuffer.ReleaseTemporaryRT(rtId);

#if false
                // If resize is needed, need to render a quad and sample it from the source.

                // It doesn't seem like there is a way to copy-and-resize part of a texture.
                // Blit first to a temporary render target, and upscale/downscale that to the read-back texture.
                var rtId = Shader.PropertyToID("tempRT");
                var rtDesc = new RenderTextureDescriptor(XRSettings.eyeTextureDesc.width, XRSettings.eyeTextureDesc.height,
                    XRSettings.eyeTextureDesc.colorFormat, depthBufferBits: 0); // make a copy to avoid error about AA
                rtDesc.autoGenerateMips = false;
                _commandBuffer.GetTemporaryRT(rtId, rtDesc, FilterMode.Point);
                var srcOffset = Vector2.zero;
                var srcScale = new Vector2(0.5f, 1.0f);
#if UNITY_2019_1_OR_NEWER
                int srcSliceIndex = 0; // left eye
                int dstSliceIndex = 0;
                _commandBuffer.Blit(BuiltinRenderTextureType.CameraTarget, rtId, srcScale, srcOffset, srcSliceIndex, dstSliceIndex);
#else
                _commandBuffer.Blit(BuiltinRenderTextureType.CameraTarget, rtId, srcScale, srcOffset);
#endif

                // CommandBuffer.ConvertTexture() doesn't work with RenderTexture. Use a full-screen triangle
                // instead, similar to what Unity is doing in their HDRP scriptable pipeline.
                if (_scaleMaterial == null)
                {
                    var scaleShader = Shader.Find("Hidden/MixedRealityWebRTC/ScaleTexture");
                    if (scaleShader == null)
                    {
                        throw new FileNotFoundException("Failed to find scale shader for SceneVideoSource.");
                    }
                    _scaleMaterial = new Material(scaleShader);
                }
                _scaleMaterial.SetTexture(s_InputTexID, rtId);
                _scaleMaterial.SetTextureOffset(s_InputTexID, srcOffset);
                _scaleMaterial.SetTextureScale(s_InputTexID, srcScale);
                //_commandBuffer.SetGlobalTexture(s_InputTexID, rtId);
                _commandBuffer.SetRenderTarget(_readBackTex, RenderBufferLoadAction.DontCare, RenderBufferStoreAction.Store);
                _commandBuffer.DrawProcedural(Matrix4x4.identity, _scaleMaterial, shaderPass: 0, MeshTopology.Triangles,
                    vertexCount: 3, instanceCount: 1, properties: null);
                _commandBuffer.ReleaseTemporaryRT(rtId);
#endif
            }
            else
            {
                // If resize is not needed, can use the GPU blit function to do buffer copy (faster).
                var srcOffset = Vector2.zero;
                var srcScale = (needHalfWidth ? new Vector2(0.5f, 1.0f) : Vector2.one);
#if UNITY_2019_1_OR_NEWER
                int srcSliceIndex = 0; // left eye
                int dstSliceIndex = 0;
                _commandBuffer.Blit(BuiltinRenderTextureType.CameraTarget, /*BuiltinRenderTextureType.CurrentActive*/_readBackTex,
                    srcScale, srcOffset,srcSliceIndex, dstSliceIndex);
#else
                _commandBuffer.Blit(BuiltinRenderTextureType.CameraTarget, /*BuiltinRenderTextureType.CurrentActive*/_readBackTex, srcScale, srcOffset);
#endif
            }
            _commandBuffer.EndSample("Blit");

            // Copy readback texture from GPU to CPU RAM asynchronously, invoking the given callback once done
            _commandBuffer.BeginSample("Readback");
            if (needResize)
            {
                _commandBuffer.RequestAsyncReadback(_convertTex, 0, TextureFormat.RGBA32, OnSceneFrameReady);
            }
            else
            {
                _commandBuffer.RequestAsyncReadback(_readBackTex, 0, TextureFormat.RGBA32, OnSceneFrameReady);
            }
            _commandBuffer.EndSample("Readback");
        }

        /// <summary>
        /// Callback invoked by the base class when the WebRTC track requires a new frame.
        /// </summary>
        /// <param name="request">The frame request to serve.</param>
        protected override void OnFrameRequested(in FrameRequest request)
        {
            // Try to dequeue a frame from the internal frame queue
            if (_frameQueue.TryDequeue(out Argb32VideoFrameStorage storage))
            {
                var frame = new Argb32VideoFrame
                {
                    width = storage.Width,
                    height = storage.Height,
                    stride = (int)storage.Width * 4
                };
                unsafe
                {
                    fixed (void* ptr = storage.Buffer)
                    {
                        // Complete the request with a view over the frame buffer (no allocation)
                        // while the buffer is pinned into memory. The native implementation will
                        // make a copy into a native memory buffer if necessary before returning.
                        frame.data = new IntPtr(ptr);
                        request.CompleteRequest(frame);
                    }
                }

                // Put the allocated buffer back in the pool for reuse
                _frameQueue.RecycleStorage(storage);
            }
            else
            {
                // No frame available, just skip this request
                //request.Discard();
            }
        }

        /// <summary>
        /// Callback invoked by the command buffer when the scene frame GPU readback has completed
        /// and the frame is available in CPU memory.
        /// </summary>
        /// <param name="request">The completed and possibly failed GPU readback request.</param>
        private void OnSceneFrameReady(AsyncGPUReadbackRequest request)
        {
            // Read back the data from GPU, if available
            if (request.hasError)
            {
                return;
            }
            NativeArray<byte> rawData = request.GetData<byte>();
            Debug.Assert(rawData.Length >= _frameSize.x * _frameSize.y * 4);
            unsafe
            {
                byte* ptr = (byte*)NativeArrayUnsafeUtility.GetUnsafePtr(rawData);

                // Enqueue a frame in the internal frame queue. This will make a copy
                // of the frame into a pooled buffer owned by the frame queue itself.
                var frame = new Argb32VideoFrame
                {
                    data = (IntPtr)ptr,
                    stride = _frameSize.x * 4,
                    width = (uint)_frameSize.x,
                    height = (uint)_frameSize.y
                };
                _frameQueue.Enqueue(frame);
            }
        }
    }
}


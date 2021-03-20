// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// Abstract base component for a custom video source delivering raw video frames
    /// directly to the WebRTC implementation.
    /// </summary>
    public abstract class CustomVideoSource<T> : VideoTrackSource where T : IVideoFrameStorage
    {
        [Tooltip("Capture framerate, in frames per second.")]
        [Range(1.0e-6f, 240.0f)]
        [SerializeField]
        private float _captureFramerate = 30.0f;

        /// <summary>
        /// Capture framerate of the video track source, in frames per second.
        /// This sets the frequency at which frames are produced.
        /// </summary>
        public float CaptureFramerate
        {
            get => _captureFramerate;
            set
            {
                _captureFramerate = value;
                if (_externalVideoTrackSource != null)
                {
                    _externalVideoTrackSource.Framerate = _captureFramerate;
#if UNITY_EDITOR
                    _lastCaptureFramerate = _captureFramerate;
#endif
                }
            }
        }

        private ExternalVideoTrackSource _externalVideoTrackSource = null;

        protected virtual void OnEnable()
        {
            System.Diagnostics.Debug.Assert(Source == null);

            // Create the external source
            //< TODO - Better abstraction
            if (typeof(T) == typeof(I420AVideoFrameStorage))
            {
                _externalVideoTrackSource = ExternalVideoTrackSource.CreateFromI420ACallback(OnFrameRequested);
            }
            else if (typeof(T) == typeof(Argb32VideoFrameStorage))
            {
                _externalVideoTrackSource = ExternalVideoTrackSource.CreateFromArgb32Callback(OnFrameRequested);
            }
            else
            {
                throw new NotSupportedException("This frame storage is not supported. Use I420AVideoFrameStorage or Argb32VideoFrameStorage.");
            }
            _captureFramerate = Mathf.Clamp(_captureFramerate, 1.0e-6f, 240.0f);
            _externalVideoTrackSource.Framerate = _captureFramerate;
#if UNITY_EDITOR
            _lastCaptureFramerate = _captureFramerate;
#endif
            AttachSource(_externalVideoTrackSource);
        }

        protected virtual void OnDisable()
        {
            DisposeSource();
            _externalVideoTrackSource = null;
        }

#if UNITY_EDITOR
        /// <summary>Last capture framerate assigned to <see cref="_externalVideoTrackSource"/>.</summary>
        private float _lastCaptureFramerate = -1.0f;

        protected virtual void Update()
        {
            // Allow dynamic update in Editor by monitoring changes done via the Inspector window
            if (_lastCaptureFramerate != _captureFramerate)
            {
                _lastCaptureFramerate = _captureFramerate;
                _externalVideoTrackSource.Framerate = _captureFramerate;
            }
        }
#endif

        protected abstract void OnFrameRequested(in FrameRequest request);
    }
}

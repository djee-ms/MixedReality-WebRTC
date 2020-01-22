// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using UnityEngine;
using UnityEngine.Events;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// Unity event corresponding to a new video stream being started.
    /// </summary>
    [Serializable]
    public class VideoStreamStartedEvent : UnityEvent<VideoSource>
    { };

    /// <summary>
    /// Unity event corresponding to an on-going video stream being stopped.
    /// </summary>
    [Serializable]
    public class VideoStreamStoppedEvent : UnityEvent<VideoSource>
    { };

    /// <summary>
    /// Base class for video sources plugging into the internal peer connection API to
    /// expose a single video stream to a renderer (<see cref="MediaPlayer"/> or custom).
    /// </summary>
    public abstract class VideoSource : MonoBehaviour
    {
        /// <summary>
        /// Enumeration of video encodings.
        /// </summary>
        public enum VideoEncoding
        {
            /// <summary>
            /// I420A video encoding with chroma (UV) halved in both directions (4:2:0),
            /// and optional Alpha plane.
            /// </summary>
            I420A,

            /// <summary>
            /// 32-bit ARGB32 video encoding with 8-bit per component, encoded as uint32 little-endian
            /// 0xAARRGGBB value, or equivalently (B,G,R,A) in byte order.
            /// </summary>
            Argb32
        }

        /// <summary>
        /// Video encoding indicating the kind of frames the source is producing.
        /// This is used for example by the <see cref="MediaPlayer"/> to determine how to
        /// render the frame.
        /// </summary>
        public VideoEncoding FrameEncoding { get; protected set; } = VideoEncoding.I420A;

        /// <summary>
        /// Event invoked from the main Unity thread when the video stream starts.
        /// This means that video frames are available and the renderer should start polling.
        /// </summary>
        public VideoStreamStartedEvent VideoStreamStarted = new VideoStreamStartedEvent();

        /// <summary>
        /// Event invoked from the main Unity thread when the video stream stops.
        /// This means that the video frame queue is not populated anymore, though some frames
        /// may still be present in it that may be rendered.
        /// </summary>
        public VideoStreamStoppedEvent VideoStreamStopped = new VideoStreamStoppedEvent();

        /// <summary>
        /// Retrieve some statistics about the video feed produced by the source.
        /// </summary>
        /// <returns>The current statistics for the source at the time of the call.</returns>
        public abstract IVideoFrameQueue GetStats();

        public abstract void RegisterCallback(I420AVideoFrameDelegate callback);
        public abstract void UnregisterCallback(I420AVideoFrameDelegate callback);
        public abstract void RegisterCallback(Argb32VideoFrameDelegate callback);
        public abstract void UnregisterCallback(Argb32VideoFrameDelegate callback);
    }
}

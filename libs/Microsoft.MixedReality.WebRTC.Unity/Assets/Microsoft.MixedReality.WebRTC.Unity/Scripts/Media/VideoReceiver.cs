// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Collections.Concurrent;
using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// This component represents a remote video source added as a video track to an
    /// existing WebRTC peer connection by a remote peer and received locally.
    /// The video track can optionally be displayed locally with a <see cref="MediaPlayer"/>.
    /// </summary>
    [AddComponentMenu("MixedReality-WebRTC/Video Receiver")]
    public class VideoReceiver : MediaReceiver, IVideoSource
    {
        /// <summary>
        /// Remote video track receiving data from the remote peer.
        /// </summary>
        public RemoteVideoTrack Track { get; private set; } = null;

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

        public VideoStreamStartedEvent GetVideoStreamStarted() { return VideoStreamStarted; }
        public VideoStreamStoppedEvent GetVideoStreamStopped() { return VideoStreamStopped; }

        /// <inheritdoc/>
        public VideoEncoding FrameEncoding { get; } = VideoEncoding.I420A;

        public void RegisterCallback(I420AVideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.I420AVideoFrameReady += callback;
            }
        }

        public void UnregisterCallback(I420AVideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.I420AVideoFrameReady -= callback;
            }
        }

        public void RegisterCallback(Argb32VideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.Argb32VideoFrameReady += callback;
            }
        }

        public void UnregisterCallback(Argb32VideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.Argb32VideoFrameReady -= callback;
            }
        }

        protected override void OnPlaybackStarted()
        {
        }

        protected override void OnPlaybackStopped()
        {
            if (Track != null)
            {
                VideoStreamStopped.Invoke(this);

                // Track may not be added to any transceiver (e.g. no connection)
                if (Track.Transceiver != null)
                {
                    Track.Transceiver.LocalTrack = null;
                }

                // Remote tracks are owned by the peer connection (not disposable)
                Track = null;
            }
        }

        /// <summary>
        /// Free-threaded callback invoked by the owning peer connection when a track is paired
        /// with this receiver, which enqueues the <see cref="VideoSource.VideoStreamStarted"/>
        /// event to be fired from the main Unity thread.
        /// </summary>
        internal void OnPaired(RemoteVideoTrack track)
        {
            Debug.Assert(Track == null);
            Track = track;

            // Enqueue invoking the unity event from the main Unity thread, so that listeners
            // can directly access Unity objects from their handler function.
            _mainThreadWorkQueue.Enqueue(() => VideoStreamStarted.Invoke(this));
        }

        /// <summary>
        /// Free-threaded callback invoked by the owning peer connection when a track is unpaired
        /// from this receiver, which enqueues the <see cref="VideoSource.VideoStreamStopped"/>
        /// event to be fired from the main Unity thread.
        /// </summary>
        internal void OnUnpaired(RemoteVideoTrack track)
        {
            Debug.Assert(Track == track);
            Track = null;

            // Enqueue invoking the unity event from the main Unity thread, so that listeners
            // can directly access Unity objects from their handler function.
            _mainThreadWorkQueue.Enqueue(() => VideoStreamStopped.Invoke(this));
        }
    }
}

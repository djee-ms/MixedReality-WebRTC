﻿// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System.Threading.Tasks;
using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// This component represents a local video source added as a video track to an
    /// existing WebRTC peer connection and sent to the remote peer. The video track
    /// can optionally be displayed locally with a <see cref="MediaPlayer"/>.
    /// </summary>
    [AddComponentMenu("MixedReality-WebRTC/Video Sender")]
    public abstract class VideoSender : MediaSender, IVideoSource
    {
        /// <summary>
        /// Automatically start local video capture when this component is enabled.
        /// </summary>
        [Header("Local video capture")]
        [Tooltip("Automatically start local video capture when this component is enabled")]
        public bool AutoStartCapture = true;

        /// <summary>
        /// Name of the preferred video codec, or empty to let WebRTC decide.
        /// See https://en.wikipedia.org/wiki/RTP_audio_video_profile for the standard SDP names.
        /// </summary>
        [Tooltip("SDP name of the preferred video codec to use if supported")]
        public string PreferredVideoCodec = string.Empty;

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

        /// <summary>
        /// Video track added to the peer connection that this component encapsulates.
        /// </summary>
        public LocalVideoTrack Track { get; protected set; }

        public VideoSender(VideoEncoding frameEncoding)
        {
            FrameEncoding = frameEncoding;
        }

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

        public async Task AddTrackAsync()
        {
            if (Track == null)
            {
                await DoAddTrackAsyncAction();
                Debug.Assert(Track != null, "Implementation did not create a valid Track property yet did not throw any exception.", this);
                //Debug.Assert(Track.Enabled == enabled, "Track.Enabled is not in sync with component's enabled property.", this);
                VideoStreamStarted.Invoke(this);
            }
        }

        protected override void OnMediaInitialized()
        {
            if (AutoStartCapture && isActiveAndEnabled)
            {
                _ = AddTrackAsync();
            }
        }

        protected override void OnMediaShutdown()
        {
            if (Track != null)
            {
                VideoStreamStopped.Invoke(this);
                DoRemoveTrackAction();
            }
        }

        protected override void MuteImpl(bool mute)
        {
            if (Track != null)
            {
                Track.Enabled = mute;
            }
        }

        /// <summary>
        /// Implement this callback to create the <see cref="Track"/> instance.
        /// On failure, this method must throw. Otherwise it must return a non-null track.
        /// The track must be enabled according to the <see xref="UnityEngine.Behaviour.enabled"/>
        /// property of the Unity component.
        /// </summary>
        protected abstract Task DoAddTrackAsyncAction();

        /// <summary>
        /// Re-implement this callback to destroy the <see cref="Track"/> instance
        /// and other associated resources.
        /// </summary>
        protected virtual void DoRemoveTrackAction()
        {
            if (Track != null)
            {
                VideoStreamStopped.Invoke(this);

                // Track may not be added to any transceiver (e.g. no connection)
                if (Track.Transceiver != null)
                {
                    Track.Transceiver.LocalTrack = null;
                }

                // Local tracks are disposable objects
                Track.Dispose();
                Track = null;
            }
        }
    }
}

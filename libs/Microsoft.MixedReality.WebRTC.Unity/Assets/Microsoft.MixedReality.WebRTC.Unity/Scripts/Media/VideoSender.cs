// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// This component represents a local video source added as a video track to an
    /// existing WebRTC peer connection and sent to the remote peer. The video track
    /// can optionally be displayed locally with a <see cref="MediaPlayer"/>.
    /// </summary>
    [AddComponentMenu("MixedReality-WebRTC/Video Sender")]
    public abstract class VideoSender : VideoSource
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
        /// Peer connection this local video source will add a video track to.
        /// </summary>
        [Header("Video track")]
        public PeerConnection PeerConnection;

        /// <summary>
        /// Name of the track.
        /// </summary>
        /// <remarks>
        /// This must comply with the 'msid' attribute rules as defined in
        /// https://tools.ietf.org/html/draft-ietf-mmusic-msid-05#section-2, which in
        /// particular constraints the set of allows characters to those allowed for a
        /// 'token' element as specified in https://tools.ietf.org/html/rfc4566#page-43:
        /// - Symbols [!#$%'*+-.^_`{|}~] and ampersand &amp;
        /// - Alphanumerical [A-Za-z0-9]
        /// </remarks>
        /// <seealso xref="SdpTokenAttribute.ValidateSdpTokenName"/>
        [Tooltip("SDP track name.")]
        [SdpToken(allowEmpty: true)]
        public string TrackName;

        /// <summary>
        /// Automatically register as a video track when the peer connection is ready and the
        /// component is enabled.
        /// </summary>
        /// <remarks>
        /// If this is <c>false</c> then the user needs to manually call <cref href="AddTrack"/> to add
        /// a video track to the peer connection and start sending video data to the remote peer.
        /// </remarks>
        [Tooltip("Automatically add the video track to the peer connection on Start")]
        public bool AutoAddTrackOnStart = true;

        /// <summary>
        /// Video track added to the peer connection that this component encapsulates.
        /// </summary>
        public LocalVideoTrack Track { get; protected set; }

        public VideoSender(VideoEncoding frameEncoding = VideoEncoding.I420A) : base(frameEncoding)
        {
        }

        public override void RegisterCallback(I420AVideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.I420AVideoFrameReady += callback;
            }
        }

        public override void UnregisterCallback(I420AVideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.I420AVideoFrameReady -= callback;
            }
        }

        public override void RegisterCallback(Argb32VideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.Argb32VideoFrameReady += callback;
            }
        }

        public override void UnregisterCallback(Argb32VideoFrameDelegate callback)
        {
            if (Track != null)
            {
                Track.Argb32VideoFrameReady -= callback;
            }
        }

        public void AddTrack()
        {
            if (Track == null)
            {
                DoAddTrackAction();
                Debug.Assert(Track != null, "Implementation did not create a valid Track property yet did not throw any exception.", this);
                Debug.Assert(Track.Enabled == enabled, "Track.Enabled is not in sync with component's enabled property.", this);
                VideoStreamStarted.Invoke(this);
            }
        }

        public void RemoveTrack()
        {
            if (Track != null)
            {
                VideoStreamStopped.Invoke(this);
                DoRemoveTrackAction();
            }
        }

        protected void Awake()
        {
            PeerConnection.RegisterSource(this);
            PeerConnection.OnInitialized.AddListener(OnPeerInitialized);
            PeerConnection.OnShutdown.AddListener(OnPeerShutdown);
        }

        protected void OnDestroy()
        {
            PeerConnection.OnInitialized.RemoveListener(OnPeerInitialized);
            PeerConnection.OnShutdown.RemoveListener(OnPeerShutdown);
            RemoveTrack();
        }

        protected void OnEnable()
        {
            if (Track != null)
            {
                Track.Enabled = true;
            }
        }

        protected void OnDisable()
        {
            if (Track != null)
            {
                Track.Enabled = false;
            }
        }

        private void OnPeerInitialized()
        {
            var nativePeer = PeerConnection.Peer;
            nativePeer.PreferredVideoCodec = PreferredVideoCodec;

            if (AutoAddTrackOnStart)
            {
                AddTrack();
            }
        }

        private void OnPeerShutdown()
        {
            RemoveTrack();
        }

        /// <summary>
        /// Implement this callback to create the <see cref="Track"/> instance.
        /// On failure, this method must throw. Otherwise it must return a non-null track.
        /// The track must be enabled according to the <see xref="UnityEngine.Behaviour.enabled"/>
        /// property of the Unity component.
        /// </summary>
        protected abstract void DoAddTrackAction();

        /// <summary>
        /// Re-implement this callback to destroy the <see cref="Track"/> instance
        /// and other associated resources.
        /// </summary>
        protected virtual void DoRemoveTrackAction()
        {
            if (Track != null)
            {
                Track.Transceiver.LocalTrack = null;
                Track.Dispose();
                Track = null;
            }
        }
    }
}

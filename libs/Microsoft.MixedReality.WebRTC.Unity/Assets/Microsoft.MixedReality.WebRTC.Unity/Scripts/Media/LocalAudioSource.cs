// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// This component represents a local audio source added as an audio track to an
    /// existing WebRTC peer connection and sent to the remote peer. The audio track
    /// can optionally be rendered locally with a <see cref="MediaPlayer"/>.
    /// </summary>
    [AddComponentMenu("MixedReality-WebRTC/Local Audio Source")]
    public class LocalAudioSource : AudioSource
    {
        /// <summary>
        /// Automatically start local audio capture when this component is enabled.
        /// </summary>
        [Header("Local audio capture")]
        [Tooltip("Automatically start local audio capture when this component is enabled")]
        public bool AutoStartCapture = true;

        /// <summary>
        /// Name of the preferred audio codec, or empty to let WebRTC decide.
        /// See https://en.wikipedia.org/wiki/RTP_audio_video_profile for the standard SDP names.
        /// </summary>
        [Tooltip("SDP name of the preferred audio codec to use if supported")]
        public string PreferredAudioCodec = string.Empty;

        /// <summary>
        /// Peer connection this local audio source will add an audio track to.
        /// </summary>
        [Header("Audio track")]
        public PeerConnection PeerConnection;

        /// <summary>
        /// Automatically register as an audio track when the peer connection is ready.
        /// </summary>
        public bool AutoAddTrack = true;

        public LocalAudioTrack Track { get; private set; } = null;

        /// <summary>
        /// Frame queue holding the pending frames enqueued by the audio source itself,
        /// which an audio renderer needs to read and display.
        /// </summary>
        //private AudioFrameQueue<AudioFrameStorage> _frameQueue;

        protected void Awake()
        {
            PeerConnection.OnInitialized.AddListener(OnPeerInitialized);
            PeerConnection.OnShutdown.AddListener(OnPeerShutdown);
        }

        protected void OnDestroy()
        {
            PeerConnection.OnInitialized.RemoveListener(OnPeerInitialized);
            PeerConnection.OnShutdown.RemoveListener(OnPeerShutdown);
        }

        protected void OnEnable()
        {
            var nativePeer = PeerConnection?.Peer;
            if ((nativePeer != null) && nativePeer.Initialized)
            {
                DoAutoStartActions(nativePeer);
            }
        }

        protected void OnDisable()
        {
            var nativePeer = PeerConnection.Peer;
            if ((Track != null) && (nativePeer != null) && nativePeer.Initialized)
            {
                AudioStreamStopped.Invoke();
                //Track.AudioFrameReady -= AudioFrameReady;
                nativePeer.RemoveLocalAudioTrack(Track);
                Track.Dispose();
                Track = null;
                //_frameQueue.Clear();
            }
        }

        private void OnPeerInitialized()
        {
            var nativePeer = PeerConnection.Peer;
            nativePeer.PreferredAudioCodec = PreferredAudioCodec;

            // Only perform auto-start actions (add track, start capture) if the component
            // is enabled. Otherwise just do nothing, this component is idle.
            if (enabled)
            {
                DoAutoStartActions(nativePeer);
            }
        }

        private async void DoAutoStartActions(WebRTC.PeerConnection nativePeer)
        {
            if (AutoStartCapture)
            {
                //nativePeer.LocalAudioFrameReady += LocalAudioFrameReady;

                // TODO - Currently AddLocalAudioTrackAsync() both open the capture device AND add an audio track
            }

            if (AutoAddTrack)
            {
                // Force again PreferredAudioCodec right before starting the local capture,
                // so that modifications to the property done after OnPeerInitialized() are
                // accounted for.
                nativePeer.PreferredAudioCodec = PreferredAudioCodec;

                //FrameQueue.Clear();
                var settings = new WebRTC.PeerConnection.LocalAudioTrackSettings
                {
                    trackName = "LocalAudioSource", //< TODO
                };
                Track = await nativePeer.AddLocalAudioTrackAsync(settings);
                AudioStreamStarted.Invoke();
            }
        }

        private void OnPeerShutdown()
        {
            if (Track != null)
            {
                AudioStreamStopped.Invoke();
                //Track.AudioFrameReady -= AudioFrameReady;
                var nativePeer = PeerConnection.Peer;
                nativePeer.RemoveLocalAudioTrack(Track);
                Track.Dispose();
                Track = null;
            }
            //_frameQueue.Clear();
        }

        //private void AudioFrameReady(AudioFrame frame)
        //{
        //    _frameQueue.Enqueue(frame);
        //}
    }
}

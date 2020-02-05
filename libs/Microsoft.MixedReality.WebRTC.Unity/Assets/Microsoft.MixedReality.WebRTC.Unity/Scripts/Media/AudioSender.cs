// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// This component represents a local audio source added as an audio track to an
    /// existing WebRTC peer connection and sent to the remote peer. The audio track
    /// can optionally be rendered locally with a <see cref="MediaPlayer"/>.
    /// </summary>
    [AddComponentMenu("MixedReality-WebRTC/Audio Sender")]
    public class AudioSender : MediaSender, IAudioSource
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

        public AudioStreamStartedEvent AudioStreamStarted = new AudioStreamStartedEvent();
        public AudioStreamStoppedEvent AudioStreamStopped = new AudioStreamStoppedEvent();

        public AudioStreamStartedEvent GetAudioStreamStarted() { return AudioStreamStarted; }
        public AudioStreamStoppedEvent GetAudioStreamStopped() { return AudioStreamStopped; }

        /// <summary>
        /// Audio track added to the peer connection that this component encapsulates.
        /// </summary>
        public LocalAudioTrack Track { get; private set; } = null;

        public void RegisterCallback(AudioFrameDelegate callback) { }
        public void UnregisterCallback(AudioFrameDelegate callback) { }

        protected override void MuteImpl(bool mute)
        {
            if (Track != null)
            {
                Track.Enabled = mute;
            }
        }

        //protected void OnEnable()
        //{
        //    var nativePeer = PeerConnection?.Peer;
        //    if ((nativePeer != null) && nativePeer.Initialized)
        //    {
        //        DoStartAction();
        //    }
        //}

        //protected void OnDisable()
        //{
        //    var nativePeer = PeerConnection.Peer;
        //    if ((Track != null) && (nativePeer != null) && nativePeer.Initialized)
        //    {
        //        AudioStreamStopped.Invoke();
        //        //Track.AudioFrameReady -= AudioFrameReady;
        //        Track.Transceiver.LocalTrack = null; //nativePeer.RemoveLocalAudioTrack(Track);
        //        Track.Dispose();
        //        Track = null;
        //        //_frameQueue.Clear();
        //    }
        //}

        protected override async void OnMediaInitialized()
        {
            PeerConnection.Peer.PreferredAudioCodec = PreferredAudioCodec;

            if (AutoStartCapture)
            {
                //nativePeer.LocalAudioFrameReady += LocalAudioFrameReady;

                // TODO - Currently AddLocalAudioTrackAsync() both open the capture device AND add an audio track
            }

            if (AutoAddTrack)
            {
                var nativePeer = PeerConnection.Peer;
                Debug.Assert(nativePeer.Initialized);

                // Force again PreferredAudioCodec right before starting the local capture,
                // so that modifications to the property done after OnPeerInitialized() are
                // accounted for.
                nativePeer.PreferredAudioCodec = PreferredAudioCodec;

                // Ensure the track has a valid name
                string trackName = TrackName;
                if (trackName.Length == 0)
                {
                    trackName = Guid.NewGuid().ToString();
                    TrackName = trackName;
                }
                SdpTokenAttribute.Validate(trackName, allowEmpty: false);

                var transceiverSettings = new AudioTransceiverInitSettings
                {
                    InitialDesiredDirection = Transceiver.Direction.SendReceive,
                    // Use the same name for the track and the transceiver
                    Name = trackName
                };
                var transceiver = nativePeer.AddAudioTransceiver(transceiverSettings);

                var trackSettings = new LocalAudioTrackSettings
                {
                    trackName = trackName
                };
                Track = await LocalAudioTrack.CreateFromDeviceAsync(trackSettings);

                // Set the track on the transceiver to start streaming media to the remote peer
                if (Track != null)
                {
                    transceiver.LocalTrack = Track;
                    AudioStreamStarted.Invoke();
                }
            }
        }

        protected override void OnMediaShutdown()
        {
            if (Track != null)
            {
                AudioStreamStopped.Invoke();

                // Track may not be added to any transceiver (e.g. no connection)
                if (Track.Transceiver != null)
                {
                    Track.Transceiver.LocalTrack = null;
                }

                // Local tracks are disposable objects
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

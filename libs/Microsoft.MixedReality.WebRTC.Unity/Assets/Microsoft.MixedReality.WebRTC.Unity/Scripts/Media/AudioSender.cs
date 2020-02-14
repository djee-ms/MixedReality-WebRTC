// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Threading.Tasks;
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

        public AudioTransceiver Transceiver { get; private set; }

        /// <summary>
        /// Audio track added to the peer connection that this component encapsulates.
        /// </summary>
        public LocalAudioTrack Track { get; private set; } = null;

        /// <summary>
        /// Register a frame callback to listen to outgoing audio data produced by this audio sender
        /// and sent to the remote peer.
        /// </summary>
        /// <param name="callback">The new frame callback to register.</param>
        /// <remarks>
        /// Unlike for video, where a typical application might display some local feedback of a local
        /// webcam recording, local microphone feedback is rare, so this callback is not typically used.
        /// 
        /// Note that registering a callback does not influence the audio capture and sending to the
        /// remote peer, which occurs whether or not a callback is registered.
        /// </remarks>
        public void RegisterCallback(AudioFrameDelegate callback) { }

        /// <summary>
        /// Unregister an existing frame callback registered with <see cref="RegisterCallback(AudioFrameDelegate)"/>.
        /// </summary>
        /// <param name="callback">The frame callback to unregister.</param>
        public void UnregisterCallback(AudioFrameDelegate callback) { }

        protected override async Task DoStartMediaPlaybackAsync()
        {
            if (Track == null)
            {
                // Ensure the track has a valid name
                string trackName = TrackName;
                if (trackName.Length == 0)
                {
                    trackName = Guid.NewGuid().ToString();
                    TrackName = trackName;
                }
                SdpTokenAttribute.Validate(trackName, allowEmpty: false);

                // Create the local track
                var trackSettings = new LocalAudioTrackSettings
                {
                    trackName = trackName
                };
                Track = await LocalAudioTrack.CreateFromDeviceAsync(trackSettings);

                AudioStreamStarted.Invoke();
            }
        }

        protected override void DoStopMediaPlayback()
        {
            if (Track != null)
            {
                AudioStreamStopped.Invoke();

                // Track may not be added to any transceiver (e.g. no connection)
                if (Track.Transceiver != null)
                {
                    Track.Transceiver.LocalTrack = null;
                }

                // Local tracks are disposable objects owned by the user (this component)
                Track.Dispose();
                Track = null;
            }
        }

        /// <summary>
        /// Internal callback invoked when a peer connection is about to create an offer,
        /// and needs to create the audio transceivers and senders. The audio sender must
        /// create a local audio track and attach it to the given transceiver.
        /// </summary>
        /// <param name="audioTransceiver">The audio transceiver to attach a local track to.</param>
        /// <returns>Once the asynchronous operation is completed, the <see cref="Track"/> property
        /// must reference it.</returns>
        internal async Task InitAndAttachTrackAsync(AudioTransceiver audioTransceiver)
        {
            Debug.Assert(Transceiver == null);
            Transceiver = audioTransceiver;

            // Force again PreferredAudioCodec right before starting the local capture,
            // so that modifications to the property done after OnPeerInitialized() are
            // accounted for.
            //< FIXME - Multi-track override!!!
            Transceiver.PeerConnection.PreferredAudioCodec = PreferredAudioCodec;

            // Create the track if needed
            await DoStartMediaPlaybackAsync();

            // Attach the local track to the transceiver
            if (Track != null)
            {
                Transceiver.LocalTrack = Track;
            }
        }

        /// <inheritdoc/>
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

        //protected override async void OnMediaInitialized()
        //{
        //    PeerConnection.Peer.PreferredAudioCodec = PreferredAudioCodec;

        //    if (AutoStartCapture)
        //    {
        //        //nativePeer.LocalAudioFrameReady += LocalAudioFrameReady;

        //        // TODO - Currently AddLocalAudioTrackAsync() both open the capture device AND add an audio track
        //    }

        //    if (AutoAddTrack)
        //    {
        //        var nativePeer = PeerConnection.Peer;
        //        Debug.Assert(nativePeer.Initialized);

        //        // Force again PreferredAudioCodec right before starting the local capture,
        //        // so that modifications to the property done after OnPeerInitialized() are
        //        // accounted for.
        //        nativePeer.PreferredAudioCodec = PreferredAudioCodec;

        //        // Ensure the track has a valid name
        //        string trackName = TrackName;
        //        if (trackName.Length == 0)
        //        {
        //            trackName = Guid.NewGuid().ToString();
        //            TrackName = trackName;
        //        }
        //        SdpTokenAttribute.Validate(trackName, allowEmpty: false);

        //        var transceiverSettings = new AudioTransceiverInitSettings
        //        {
        //            InitialDesiredDirection = Transceiver.Direction.SendReceive,
        //            // Use the same name for the track and the transceiver
        //            Name = trackName
        //        };
        //        var transceiver = nativePeer.AddAudioTransceiver(transceiverSettings);

        //        var trackSettings = new LocalAudioTrackSettings
        //        {
        //            trackName = trackName
        //        };
        //        Track = await LocalAudioTrack.CreateFromDeviceAsync(trackSettings);

        //        // Set the track on the transceiver to start streaming media to the remote peer
        //        if (Track != null)
        //        {
        //            transceiver.LocalTrack = Track;
        //            AudioStreamStarted.Invoke();
        //        }
        //    }
        //}

        //protected override void OnMediaShutdown()
        //{
        //    if (Track != null)
        //    {
        //        AudioStreamStopped.Invoke();

        //        // Track may not be added to any transceiver (e.g. no connection)
        //        if (Track.Transceiver != null)
        //        {
        //            Track.Transceiver.LocalTrack = null;
        //        }

        //        // Local tracks are disposable objects
        //        Track.Dispose();
        //        Track = null;
        //    }
        //    //_frameQueue.Clear();
        //}

        //private void AudioFrameReady(AudioFrame frame)
        //{
        //    _frameQueue.Enqueue(frame);
        //}
    }
}

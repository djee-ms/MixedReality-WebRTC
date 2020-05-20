// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// This component represents a local audio source added as an audio track to an
    /// existing WebRTC peer connection and sent to the remote peer.
    /// 
    /// <div class="WARNING alert alert-warning">
    /// <h5>WARNING</h5>
    /// <p>
    /// Currently the low-level WebRTC implementation does not support registering
    /// local audio callbacks, therefore the audio track cannot be rendered locally
    /// with a <see cref="MediaPlayer"/>.
    /// </p>
    /// </div>
    /// </summary>
    /// <seealso cref="MicrophoneSource"/>
    public class AudioSender : MediaSender, IDisposable
    {
        /// <summary>
        /// Name of the preferred audio codec, or empty to let WebRTC decide.
        /// See https://en.wikipedia.org/wiki/RTP_audio_video_profile for the standard SDP names.
        /// </summary>
        [Tooltip("SDP name of the preferred audio codec to use if supported")]
        [SdpToken(allowEmpty: true)]
        public string PreferredAudioCodec = string.Empty;

        /// <summary>
        /// Audio source providing frames to this audio sender.
        /// </summary>
        public AudioTrackSource Source { get; private set; } = null;

        /// <summary>
        /// Audio transceiver this sender is paired with, if any.
        /// 
        /// This is <c>null</c> until a remote description is applied which pairs the media line
        /// the sender is associated with to a transceiver.
        /// </summary>
        public Transceiver Transceiver { get; private set; }

        /// <summary>
        /// Local audio track that this component encapsulates, which if paired sends data to
        /// the remote peer.
        /// </summary>
        public LocalAudioTrack Track { get; protected set; } = null;

        public static AudioSender CreateFromSource(AudioTrackSource source, string trackName)
        {
            var initConfig = new LocalAudioTrackInitConfig
            {
                trackName = trackName
            };
            var track = LocalAudioTrack.CreateFromSource(source.Source, initConfig);
            return new AudioSender(source, track);
        }

        public void Dispose()
        {
            if (Track != null)
            {
                // Detach the local track from the transceiver
                if ((Transceiver != null) && (Transceiver.LocalAudioTrack == Track))
                {
                    Transceiver.LocalAudioTrack = null;
                }

                // Detach from source
                Debug.Assert(Source != null);
                Source.OnSenderRemoved(this);
                Source = null;

                // Local tracks are disposable objects owned by the user (this component)
                Track.Dispose();
                Track = null;
            }
        }

        protected AudioSender(AudioTrackSource source, LocalAudioTrack track) : base(MediaKind.Audio)
        {
            Source = source;
            Track = track;
            Source.OnSenderAdded(this);
        }

        protected override void CreateLocalTrack()
        {
            if (Track == null)
            {
                var initConfig = new LocalAudioTrackInitConfig
                {
                    trackName = TrackName
                };
                Track = LocalAudioTrack.CreateFromSource(Source.Source, initConfig);
            }

            // Attach the local track to the transceiver
            if (Transceiver != null)
            {
                Transceiver.LocalAudioTrack = Track;
            }
        }

        protected override void DestroyLocalTrack()
        {
            if (Track != null)
            {
                // Detach the local track from the transceiver
                if ((Transceiver != null) && (Transceiver.LocalAudioTrack == Track))
                {
                    Transceiver.LocalAudioTrack = null;
                }

                // Local tracks are disposable objects owned by the user (this component)
                Track.Dispose();
                Track = null;
            }
        }

        /// <summary>
        /// Internal callback invoked when the underlying video source is about to be destroyed.
        /// </summary>
        internal void OnSourceDestroyed()
        {
            Dispose();
            Source = null;
        }

        /// <summary>
        /// Internal callback invoked when the audio sender is attached to a transceiver created
        /// just before the peer connection creates an SDP offer.
        /// </summary>
        /// <param name="audioTransceiver">The audio transceiver this sender is attached with.</param>
        internal void AttachToTransceiver(Transceiver audioTransceiver)
        {
            Debug.Assert((Transceiver == null) || (Transceiver == audioTransceiver));
            Transceiver = audioTransceiver;
        }

        /// <summary>
        /// Internal callback invoked when the audio sender is detached from a transceiver about to be
        /// destroyed by the native implementation.
        /// </summary>
        /// <param name="audioTransceiver">The audio transceiver this sender is attached with.</param>
        internal void DetachFromTransceiver(Transceiver audioTransceiver)
        {
            Debug.Assert((Transceiver == null) || (Transceiver == audioTransceiver));
            Transceiver = null;
        }

        /// <summary>
        /// Internal callback invoked when a peer connection is about to create an offer,
        /// and needs to create the audio transceivers and senders. The audio sender must
        /// create a local audio track and attach it to the given transceiver.
        /// </summary>
        /// <returns>Once the asynchronous operation is completed, the <see cref="Track"/> property
        /// must reference it.</returns>
        internal override void AttachTrack()
        {
            Debug.Assert(Transceiver != null);
            Debug.Assert(Track != null);

            // Force again PreferredAudioCodec right before starting the local capture,
            // so that modifications to the property done after OnPeerInitialized() are
            // accounted for.
            //< FIXME - Multi-track override!!!
            if (!string.IsNullOrWhiteSpace(PreferredAudioCodec))
            {
                Transceiver.PeerConnection.PreferredAudioCodec = PreferredAudioCodec;
                Debug.LogWarning("PreferredAudioCodec is currently a per-PeerConnection setting; overriding the value for peer"
                    + $" connection '{Transceiver.PeerConnection.Name}' with track's value of '{PreferredAudioCodec}'.");
            }

            // Attach the local track to the transceiver
            Transceiver.LocalAudioTrack = Track;
        }

        internal override void DetachTrack()
        {
            Debug.Assert(Transceiver != null);
            if (Track != null)
            {
                Debug.Assert(Transceiver.LocalAudioTrack == Track);
                Transceiver.LocalAudioTrack = null;
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
    }
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Threading.Tasks;
using UnityEngine;

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// This component represents a local video source added as a video track to an
    /// existing WebRTC peer connection and sent to the remote peer. It wraps a video
    /// track source and bridges it with a video transceiver. Internally it manages
    /// a local video track (<see cref="LocalVideoTrack"/>).
    /// </summary>
    public class VideoSender : MediaSender, IDisposable
    {
        /// <summary>
        /// Name of the preferred video codec, or empty to let WebRTC decide.
        /// See https://en.wikipedia.org/wiki/RTP_audio_video_profile for the standard SDP names.
        /// </summary>
        [Tooltip("SDP name of the preferred video codec to use if supported")]
        [SdpToken(allowEmpty: true)]
        public string PreferredVideoCodec = string.Empty;

        /// <summary>
        /// Video source providing frames to this video sender.
        /// </summary>
        public VideoTrackSource Source { get; private set; } = null;

        /// <summary>
        /// Video transceiver this sender is paired with, if any.
        /// 
        /// This is <c>null</c> until a remote description is applied which pairs the media line
        /// the sender is associated with to a transceiver.
        /// </summary>
        public Transceiver Transceiver { get; private set; }

        /// <summary>
        /// Video track that this component encapsulates.
        /// </summary>
        public LocalVideoTrack Track { get; protected set; } = null;

        public static VideoSender CreateFromSource(VideoTrackSource source, string trackName)
        {
            var initConfig = new LocalVideoTrackInitConfig
            {
                trackName = trackName
            };
            var track = LocalVideoTrack.CreateFromSource(source.Source, initConfig);
            return new VideoSender(source, track);
        }

        protected VideoSender(VideoTrackSource source, LocalVideoTrack track) : base(MediaKind.Video)
        {
            Source = source;
            Track = track;
            Source.OnSenderAdded(this);
        }

        public void Dispose()
        {
            if (Track != null)
            {
                // Detach the local track from the transceiver
                if ((Transceiver != null) && (Transceiver.LocalVideoTrack == Track))
                {
                    Transceiver.LocalVideoTrack = null;
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

        /// <summary>
        /// Internal callback invoked when the underlying video source is about to be destroyed.
        /// </summary>
        internal void OnSourceDestroyed()
        {
            Dispose();
            Source = null;
        }

        /// <summary>
        /// Internal callback invoked when the video sender is attached to a transceiver created
        /// just before the peer connection creates an SDP offer.
        /// </summary>
        /// <param name="videoTransceiver">The video transceiver this sender is attached with.</param>
        internal void AttachToTransceiver(Transceiver videoTransceiver)
        {
            Debug.Assert((Transceiver == null) || (Transceiver == videoTransceiver));
            Transceiver = videoTransceiver;
        }

        /// <summary>
        /// Internal callback invoked when the video sender is detached from a transceiver about to be
        /// destroyed by the native implementation.
        /// </summary>
        /// <param name="videoTransceiver">The video transceiver this sender is attached with.</param>
        internal void DetachFromTransceiver(Transceiver videoTransceiver)
        {
            Debug.Assert((Transceiver == null) || (Transceiver == videoTransceiver));
            Transceiver = null;
        }

        internal override void AttachTrack()
        {
            Debug.Assert(Transceiver != null);
            Debug.Assert(Track != null);

            // Force again PreferredVideoCodec right before starting the local capture,
            // so that modifications to the property done after OnPeerInitialized() are
            // accounted for.
            //< FIXME - Multi-track override!!!
            if (!string.IsNullOrWhiteSpace(PreferredVideoCodec))
            {
                Transceiver.PeerConnection.PreferredVideoCodec = PreferredVideoCodec;
                Debug.LogWarning("PreferredVideoCodec is currently a per-PeerConnection setting; overriding the value for peer"
                    + $" connection '{Transceiver.PeerConnection.Name}' with track's value of '{PreferredVideoCodec}'.");
            }

            // Attach the local track to the transceiver
            Transceiver.LocalVideoTrack = Track;
        }

        internal override void DetachTrack()
        {
            Debug.Assert(Transceiver != null);
            if (Track != null)
            {
                Debug.Assert(Transceiver.LocalVideoTrack == Track);
                Transceiver.LocalVideoTrack = null;
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

        /// <summary>
        /// Implement this callback to create the <see cref="Track"/> instance.
        /// On failure, this method must throw an exception. Otherwise it must set the <see cref="Track"/>
        /// property to a non-<c>null</c> instance.
        /// </summary>
        protected Task CreateLocalVideoTrackAsync() { return Task.CompletedTask; }

        /// <summary>
        /// Re-implement this callback to destroy the <see cref="Track"/> instance
        /// and other associated resources.
        /// </summary>
        protected virtual void DestroyLocalVideoTrack()
        {
            if (Track != null)
            {
                // Track may not be added to any transceiver (e.g. no connection), or the
                // transceiver is about to be destroyed so the DetachFromTransceiver() already
                // cleared it.
                var transceiver = Transceiver;
                if (transceiver != null)
                {
                    if (transceiver.LocalVideoTrack != null)
                    {
                        Debug.Assert(transceiver.LocalVideoTrack == Track);
                        transceiver.LocalVideoTrack = null;
                    }
                }

                // Local tracks are disposable objects owned by the user (this component)
                Track.Dispose();
                Track = null;
            }
        }

        protected override void CreateLocalTrack()
        {
            throw new System.NotImplementedException();
        }

        protected override void DestroyLocalTrack()
        {
            throw new System.NotImplementedException();
        }
    }
}

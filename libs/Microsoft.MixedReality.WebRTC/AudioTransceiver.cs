// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using Microsoft.MixedReality.WebRTC.Interop;
using System;
using System.Diagnostics;

namespace Microsoft.MixedReality.WebRTC
{
    /// <summary>
    /// Transceiver for audio tracks.
    /// </summary>
    /// <remarks>
    /// Note that unlike most other objects in this library, transceivers are not disposable,
    /// and are always alive after being added to a peer connection until that peer connection
    /// is disposed of. See <see cref="Transceiver"/> for details.
    /// </remarks>
    public class AudioTransceiver : Transceiver
    {
        /// <summary>
        /// Local audio track associated with the transceiver and sending some audio to the remote peer.
        /// This may be <c>null</c> if the transceiver is currently only receiving, or is inactive.
        /// </summary>
        public LocalAudioTrack LocalTrack
        {
            get { return _localTrack; }
            set { SetLocalTrack(value); }
        }

        /// <summary>
        /// Remote audio track associated with the transceiver and receiving some audio from the remote peer.
        /// This may be <c>null</c> if the transceiver is currently only sending, or is inactive.
        /// </summary>
        public RemoteAudioTrack RemoteTrack
        {
            get { return _remoteTrack; }
            set { SetRemoteTrack(value); }
        }

        /// <summary>
        /// Handle to the native AudioTransceiver object.
        /// </summary>
        /// <remarks>
        /// In native land this is a <code>Microsoft::MixedReality::WebRTC::AudioTransceiverHandle</code>.
        /// </remarks>
        protected AudioTransceiverHandle _nativeHandle = new AudioTransceiverHandle();

        private LocalAudioTrack _localTrack = null;
        private RemoteAudioTrack _remoteTrack = null;

        internal AudioTransceiver(AudioTransceiverHandle nativeHandle, PeerConnection peerConnection)
            : base(peerConnection)
        {
            _nativeHandle = nativeHandle;
        }

        /// <summary>
        /// Change the local audio track sending data to the remote peer.
        /// This detach the previous local audio track if any, and attach the new one instead.
        /// </summary>
        /// <param name="track">The new local audio track sending data to the remote peer.</param>
        public void SetLocalTrack(LocalAudioTrack track)
        {
            throw new NotImplementedException(); //< TODO
        }

        /// <summary>
        /// Change the remote audio track receiving data from the remote peer.
        /// This detach the previous remote audio track if any, and attach the new one instead.
        /// </summary>
        /// <param name="track">The new remote audio track receiving data from the remote peer.</param>
        public void SetRemoteTrack(RemoteAudioTrack track)
        {
            throw new NotImplementedException(); //< TODO
        }

        internal void OnLocalTrackCreated(LocalAudioTrack track)
        {
            Debug.Assert(_localTrack == null);
            _localTrack = track;
        }

        internal void OnLocalTrackRemoved(LocalAudioTrack track)
        {
            Debug.Assert(_localTrack == track);
            _localTrack = null;
        }

        internal void OnRemoteTrackRemoved(RemoteAudioTrack track)
        {
            Debug.Assert(_remoteTrack == track);
            _remoteTrack = null;
        }
    }
}

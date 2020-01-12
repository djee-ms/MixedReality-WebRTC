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

        // Constructor for interop-based creation; SetHandle() will be called later
        internal AudioTransceiver(PeerConnection peerConnection)
            : base(peerConnection)
        {
        }

        internal void SetHandle(AudioTransceiverHandle handle)
        {
            Debug.Assert(!handle.IsClosed);
            // Either first-time assign or no-op (assign same value again)
            Debug.Assert(_nativeHandle.IsInvalid || (_nativeHandle == handle));
            if (_nativeHandle != handle)
            {
                _nativeHandle = handle;
            }
        }

        /// <summary>
        /// Change the local audio track sending data to the remote peer.
        /// This detach the previous local audio track if any, and attach the new one instead.
        /// </summary>
        /// <param name="track">The new local audio track sending data to the remote peer.</param>
        public void SetLocalTrack(LocalAudioTrack track)
        {
            if (track.PeerConnection != PeerConnection)
            {
                throw new InvalidOperationException($"Cannot set track {track} of peer connection {track.PeerConnection} on audio transceiver {this} of different peer connection {PeerConnection}.");
            }
            if (track == _localTrack)
            {
                return;
            }
            var res = AudioTransceiverInterop.AudioTransceiver_SetLocalTrack(_nativeHandle, track._nativeHandle);
            Utils.ThrowOnErrorCode(res);
            var peerConnection = PeerConnection; // this gets reset below
            _localTrack.OnTrackRemoved(peerConnection);
            _localTrack = track;
            _localTrack.OnTrackAdded(peerConnection, this);
            Debug.Assert(_localTrack.PeerConnection == PeerConnection);
            Debug.Assert(_localTrack.Transceiver == this);
            Debug.Assert(_localTrack.Transceiver.LocalTrack == _localTrack);
            switch (DesiredDirection)
            {
            case Direction.Inactive:
            case Direction.ReceiveOnly:
                if (_localTrack != null)
                {
                    // Add send bit
                    DesiredDirection |= Direction.SendOnly;
                }
                break;
            case Direction.SendOnly:
            case Direction.SendReceive:
                if (_localTrack == null)
                {
                    // Remove send bit
                    DesiredDirection &= Direction.ReceiveOnly;
                }
                break;
            }
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

        internal void OnLocalTrackAdded(LocalAudioTrack track)
        {
            Debug.Assert(_localTrack == null);
            _localTrack = track;
        }

        internal void OnLocalTrackRemoved(LocalAudioTrack track)
        {
            Debug.Assert(_localTrack == track);
            _localTrack = null;
        }

        internal void OnRemoteTrackAdded(RemoteAudioTrack track)
        {
            Debug.Assert(_remoteTrack == null);
            _remoteTrack = track;
        }

        internal void OnRemoteTrackRemoved(RemoteAudioTrack track)
        {
            Debug.Assert(_remoteTrack == track);
            _remoteTrack = null;
        }
    }
}

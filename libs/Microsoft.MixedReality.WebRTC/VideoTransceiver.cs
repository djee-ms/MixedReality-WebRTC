// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using Microsoft.MixedReality.WebRTC.Interop;
using System;
using System.Diagnostics;

namespace Microsoft.MixedReality.WebRTC
{
    /// <summary>
    /// Transceiver for video tracks.
    /// </summary>
    /// <remarks>
    /// Note that unlike most other objects in this library, transceivers are not disposable,
    /// and are always alive after being added to a peer connection until that peer connection
    /// is disposed of. See <see cref="Transceiver"/> for details.
    /// </remarks>
    public class VideoTransceiver : Transceiver
    {
        /// <summary>
        /// Local video track associated with the transceiver and sending some video to the remote peer.
        /// This may be <c>null</c> if the transceiver is currently only receiving, or is inactive.
        /// </summary>
        public LocalVideoTrack LocalTrack
        {
            get { return _localTrack; }
            set { SetLocalTrack(value); }
        }

        /// <summary>
        /// Remote video track associated with the transceiver and receiving some video from the remote peer.
        /// This may be <c>null</c> if the transceiver is currently only sending, or is inactive.
        /// </summary>
        public RemoteVideoTrack RemoteTrack
        {
            get { return _remoteTrack; }
            set { SetRemoteTrack(value); }
        }

        /// <summary>
        /// Handle to the native VideoTransceiver object.
        /// </summary>
        /// <remarks>
        /// In native land this is a <code>Microsoft::MixedReality::WebRTC::VideoTransceiverHandle</code>.
        /// </remarks>
        protected VideoTransceiverHandle _nativeHandle = new VideoTransceiverHandle();

        private LocalVideoTrack _localTrack = null;
        private RemoteVideoTrack _remoteTrack = null;

        // Constructor for interop-based creation; SetHandle() will be called later
        internal VideoTransceiver(PeerConnection peerConnection)
            : base(peerConnection)
        {
        }

        internal void SetHandle(VideoTransceiverHandle handle)
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
        /// Change the local video track sending data to the remote peer.
        /// This detach the previous local video track if any, and attach the new one instead.
        /// </summary>
        /// <param name="track">The new local video track sending data to the remote peer.</param>
        public void SetLocalTrack(LocalVideoTrack track)
        {
            if (track.PeerConnection != PeerConnection)
            {
                throw new InvalidOperationException($"Cannot set track {track} of peer connection {track.PeerConnection} on video transceiver {this} of different peer connection {PeerConnection}.");
            }
            if (track == _localTrack)
            {
                return;
            }
            var res = VideoTransceiverInterop.VideoTransceiver_SetLocalTrack(_nativeHandle, track._nativeHandle);
            Utils.ThrowOnErrorCode(res);
            var peerConnection = PeerConnection; // this gets reset below
            _localTrack.OnTrackRemoved(peerConnection);
            _localTrack = track;
            _localTrack.OnTrackAdded(peerConnection, this);
            Debug.Assert(_localTrack.PeerConnection == PeerConnection);
            Debug.Assert(_localTrack.Transceiver == this);
            Debug.Assert(_localTrack.Transceiver.LocalTrack == _localTrack);
        }

        /// <summary>
        /// Change the remote video track receiving data from the remote peer.
        /// This detach the previous remote video track if any, and attach the new one instead.
        /// </summary>
        /// <param name="track">The new remote video track receiving data from the remote peer.</param>
        public void SetRemoteTrack(RemoteVideoTrack track)
        {
            throw new NotImplementedException(); //< TODO
        }

        internal void OnLocalTrackAdded(LocalVideoTrack track)
        {
            Debug.Assert(_localTrack == null);
            _localTrack = track;
        }

        internal void OnLocalTrackRemoved(LocalVideoTrack track)
        {
            Debug.Assert(_localTrack == track);
            _localTrack = null;
        }

        internal void OnRemoteTrackAdded(RemoteVideoTrack track)
        {
            Debug.Assert(_remoteTrack == null);
            _remoteTrack = track;
        }

        internal void OnRemoteTrackRemoved(RemoteVideoTrack track)
        {
            Debug.Assert(_remoteTrack == track);
            _remoteTrack = null;
        }
    }
}

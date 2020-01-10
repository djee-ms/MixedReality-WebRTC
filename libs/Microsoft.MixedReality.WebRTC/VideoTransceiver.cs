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

        internal VideoTransceiver(VideoTransceiverHandle nativeHandle, PeerConnection peerConnection)
            : base(peerConnection)
        {
            _nativeHandle = nativeHandle;
        }

        /// <summary>
        /// Change the local video track sending data to the remote peer.
        /// This detach the previous local video track if any, and attach the new one instead.
        /// </summary>
        /// <param name="track">The new local video track sending data to the remote peer.</param>
        public void SetLocalTrack(LocalVideoTrack track)
        {
            throw new NotImplementedException(); //< TODO
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

        internal void OnLocalTrackCreated(LocalVideoTrack track)
        {
            Debug.Assert(_localTrack == null);
            _localTrack = track;
        }

        internal void OnLocalTrackRemoved(LocalVideoTrack track)
        {
            Debug.Assert(_localTrack == track);
            _localTrack = null;
        }

        internal void OnRemoteTrackRemoved(RemoteVideoTrack track)
        {
            Debug.Assert(_remoteTrack == track);
            _remoteTrack = null;
        }
    }
}

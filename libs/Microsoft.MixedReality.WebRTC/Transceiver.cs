// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;

namespace Microsoft.MixedReality.WebRTC
{
    /// <summary>
    /// Transceiver of a peer connection.
    /// 
    /// A transceiver is a media "pipe" connecting the local and remote peers, and used to transmit media
    /// data (audio or video) between the peers. The transceiver is sending media data if a local track is
    /// attached to it, and is receiving media data if a remote track is attached to it. If no track is
    /// attached, the transceiver is inactive.
    /// </summary>
    /// <remarks>
    /// This object corresponds roughly to the same-named notion in the WebRTC standard when using the
    /// Unified Plan SDP semantic, with the difference that the low-level RTP transceiver implementation
    /// always has an RTP sender and an RTP receiver, but tracks can be removed from this wrapper.
    /// For Plan B, where RTP transceivers are not available, this wrapper tries to emulate the Unified Plan
    /// transceiver concept, and is therefore providing an abstraction over the WebRTC concept of transceivers.
    /// 
    /// Note that this object is not disposable because the lifetime of the native transceiver is tied to the
    /// lifetime of the peer connection (cannot be removed), and therefore the two collections
    /// <see cref="PeerConnection.AudioTransceivers"/> and <see cref="PeerConnection.VideoTransceivers"/> own
    /// their objects and should continue to be filled with the list of wrappers for the native transceivers.
    /// </remarks>
    /// <seealso cref="AudioTransceiver"/>
    /// <seealso cref="VideoTransceiver"/>
    public abstract class Transceiver
    {
        /// <summary>
        /// Transceiver direction.
        /// </summary>
        [Flags]
        public enum Direction
        {
            /// <summary>
            /// Transceiver is inactive, neither sending nor receiving any media data, and has neither local
            /// nor remote tracks attached to it.
            /// </summary>
            Inactive = 0,

            /// <summary>
            /// Transceiver has only a local track and is sending to the remote peer, but is not receiving
            /// any media from the remote peer.
            /// </summary>
            SendOnly = 0x1,

            /// <summary>
            /// Transceiver has only a remote track and is receiving from the remote peer, but is not
            /// sending any media to the remote peer.
            /// </summary>
            ReceiveOnly = 0x2,

            /// <summary>
            /// Transceiver has both a local and remote tracks and is both sending to and receiving from
            /// the remote peer connection.
            /// </summary>
            SendReceive = 0x3
        }

        /// <summary>
        /// A name for the transceiver, used for logging and debugging.
        /// </summary>
        public string Name { get; set; } = string.Empty;

        /// <summary>
        /// Peer connection this transceiver is part of.
        /// </summary>
        public PeerConnection PeerConnection { get; } = null;

        /// <summary>
        /// Transceiver direction mapping 1:1 to the configuration of the local and remote tracks.
        /// If a negotiation is pending, then this is the next direction that will be negotiated when
        /// calling <see cref="PeerConnection.CreateOffer"/> or <see cref="PeerConnection.CreateAnswer"/>.
        /// Otherwise this is equal to <see cref="NegotiatedDirection"/>.
        /// </summary>
        public Direction DesiredDirection { get; protected set; } = Direction.Inactive;

        /// <summary>
        /// Last negotiated transceiver direction. This is equal to <see cref="DesiredDirection"/>
        /// after a negotiation is completed, but remains constant until the next SDP negotiation
        /// even if local and remote tracks are changed on the transceiver.
        /// </summary>
        public Direction NegotiatedDirection { get; protected set; } = Direction.Inactive;

        /// <summary>
        /// Create a new transceiver associated with a given peer connection.
        /// </summary>
        /// <param name="peerConnection">The peer connection owning this transceiver.</param>
        protected Transceiver(PeerConnection peerConnection)
        {
            PeerConnection = peerConnection;
        }
    }
}

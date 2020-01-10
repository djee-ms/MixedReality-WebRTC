// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

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
        /// Peer connection this transceiver is part of.
        /// </summary>
        public PeerConnection PeerConnection { get; }

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

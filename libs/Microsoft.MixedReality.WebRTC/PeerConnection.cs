// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.MixedReality.WebRTC.Interop;
using Microsoft.MixedReality.WebRTC.Tracing;

[assembly: InternalsVisibleTo("Microsoft.MixedReality.WebRTC.Tests")]

namespace Microsoft.MixedReality.WebRTC
{
    /// <summary>
    /// Type of ICE candidates offered to the remote peer.
    /// </summary>
    public enum IceTransportType : int
    {
        /// <summary>
        /// No ICE candidate offered.
        /// </summary>
        None = 0,

        /// <summary>
        /// Only advertize relay-type candidates, like TURN servers, to avoid leaking the IP address of the client.
        /// </summary>
        Relay = 1,

        /// ?
        NoHost = 2,

        /// <summary>
        /// Offer all types of ICE candidates.
        /// </summary>
        All = 3
    }

    /// <summary>
    /// Bundle policy.
    /// See https://www.w3.org/TR/webrtc/#rtcbundlepolicy-enum.
    /// </summary>
    public enum BundlePolicy : int
    {
        /// <summary>
        /// Gather ICE candidates for each media type in use (audio, video, and data). If the remote endpoint is
        /// not bundle-aware, negotiate only one audio and video track on separate transports.
        /// </summary>
        Balanced = 0,

        /// <summary>
        /// Gather ICE candidates for only one track. If the remote endpoint is not bundle-aware, negotiate only
        /// one media track.
        /// </summary>
        MaxBundle = 1,

        /// <summary>
        /// Gather ICE candidates for each track. If the remote endpoint is not bundle-aware, negotiate all media
        /// tracks on separate transports.
        /// </summary>
        MaxCompat = 2
    }

    /// <summary>
    /// SDP semantic used for (re)negotiating a peer connection.
    /// </summary>
    public enum SdpSemantic : int
    {
        /// <summary>
        /// Unified plan, as standardized in the WebRTC 1.0 standard.
        /// </summary>
        UnifiedPlan = 0,

        /// <summary>
        /// Legacy Plan B, deprecated and soon removed.
        /// Only available for compatiblity with older implementations if needed.
        /// Do not use unless there is a problem with the Unified Plan.
        /// </summary>
        PlanB = 1
    }

    /// <summary>
    /// ICE server configuration (STUN and/or TURN).
    /// </summary>
    public class IceServer
    {
        /// <summary>
        /// List of TURN and/or STUN server URLs to use for NAT bypass, in order of preference.
        ///
        /// The scheme is defined in the core WebRTC implementation, and is in short:
        /// stunURI     = stunScheme ":" stun-host [ ":" stun-port ]
        /// stunScheme  = "stun" / "stuns"
        /// turnURI     = turnScheme ":" turn-host [ ":" turn-port ] [ "?transport=" transport ]
        /// turnScheme  = "turn" / "turns"
        /// </summary>
        public List<string> Urls = new List<string>();

        /// <summary>
        /// Optional TURN server username.
        /// </summary>
        public string TurnUserName = string.Empty;

        /// <summary>
        /// Optional TURN server credentials.
        /// </summary>
        public string TurnPassword = string.Empty;

        /// <summary>
        /// Format the ICE server data according to the encoded marshalling of the C++ API.
        /// </summary>
        /// <returns>The encoded string of ICE servers.</returns>
        public override string ToString()
        {
            if (Urls == null)
            {
                return string.Empty;
            }
            string ret = string.Join("\n", Urls);
            if (!string.IsNullOrEmpty(TurnUserName))
            {
                ret += $"\nusername:{TurnUserName}";
                if (!string.IsNullOrEmpty(TurnPassword))
                {
                    ret += $"\npassword:{TurnPassword}";
                }
            }
            return ret;
        }
    }

    /// <summary>
    /// Configuration to initialize a <see cref="PeerConnection"/>.
    /// </summary>
    public class PeerConnectionConfiguration
    {
        /// <summary>
        /// List of TURN and/or STUN servers to use for NAT bypass, in order of preference.
        /// </summary>
        public List<IceServer> IceServers = new List<IceServer>();

        /// <summary>
        /// ICE transport policy for the connection.
        /// </summary>
        public IceTransportType IceTransportType = IceTransportType.All;

        /// <summary>
        /// Bundle policy for the connection.
        /// </summary>
        public BundlePolicy BundlePolicy = BundlePolicy.Balanced;

        /// <summary>
        /// SDP semantic for the connection.
        /// </summary>
        /// <remarks>Plan B is deprecated, do not use it.</remarks>
        public SdpSemantic SdpSemantic = SdpSemantic.UnifiedPlan;
    }

    /// <summary>
    /// State of an ICE connection.
    /// </summary>
    /// <remarks>
    /// Due to the underlying implementation, this is currently a mix of the
    /// <see href="https://www.w3.org/TR/webrtc/#rtcicegatheringstate-enum">RTPIceGatheringState</see>
    /// and the <see href="https://www.w3.org/TR/webrtc/#rtcpeerconnectionstate-enum">RTPPeerConnectionState</see>
    /// from the WebRTC 1.0 standard.
    /// </remarks>
    /// <seealso href="https://www.w3.org/TR/webrtc/#rtcicegatheringstate-enum"/>
    /// <seealso href="https://www.w3.org/TR/webrtc/#rtcpeerconnectionstate-enum"/>
    public enum IceConnectionState : int
    {
        /// <summary>
        /// Newly created ICE connection. This is the starting state.
        /// </summary>
        New = 0,

        /// <summary>
        /// ICE connection received an offer, but transports are not writable yet.
        /// </summary>
        Checking = 1,

        /// <summary>
        /// Transports are writable.
        /// </summary>
        Connected = 2,

        /// <summary>
        /// ICE connection finished establishing.
        /// </summary>
        Completed = 3,

        /// <summary>
        /// Failed establishing an ICE connection.
        /// </summary>
        Failed = 4,

        /// <summary>
        /// ICE connection is disconnected, there is no more writable transport.
        /// </summary>
        Disconnected = 5,

        /// <summary>
        /// The peer connection was closed entirely.
        /// </summary>
        Closed = 6,
    }

    /// <summary>
    /// State of an ICE gathering process.
    /// </summary>
    /// <remarks>
    /// See <see href="https://www.w3.org/TR/webrtc/#rtcicegatheringstate-enum">RTPIceGatheringState</see>
    /// from the WebRTC 1.0 standard.
    /// </remarks>
    /// <seealso href="https://www.w3.org/TR/webrtc/#rtcicegatheringstate-enum"/>
    public enum IceGatheringState : int
    {
        /// <summary>
        /// There is no ICE transport, or none of them started gathering ICE candidates.
        /// </summary>
        New = 0,

        /// <summary>
        /// The gathering process started. At least one ICE transport is active and gathering
        /// some ICE candidates.
        /// </summary>
        Gathering = 1,

        /// <summary>
        /// The gathering process is complete. At least one ICE transport was active, and
        /// all transports finished gathering ICE candidates.
        /// </summary>
        Complete = 2,
    }

    /// <summary>
    /// Identifier for a video capture device.
    /// </summary>
    [Serializable]
    public struct VideoCaptureDevice
    {
        /// <summary>
        /// Unique device identifier.
        /// </summary>
        public string id;

        /// <summary>
        /// Friendly device name.
        /// </summary>
        public string name;
    }

    /// <summary>
    /// Capture format for a video track.
    /// </summary>
    [Serializable]
    public struct VideoCaptureFormat
    {
        /// <summary>
        /// Frame width, in pixels.
        /// </summary>
        public uint width;

        /// <summary>
        /// Frame height, in pixels.
        /// </summary>
        public uint height;

        /// <summary>
        /// Capture framerate, in frames per second.
        /// </summary>
        public double framerate;

        /// <summary>
        /// FOURCC identifier of the video encoding.
        /// </summary>
        public uint fourcc;
    }

    /// <summary>
    /// Settings to create a new transceiver wrapper.
    /// </summary>
    public class TransceiverInitSettings
    {
        /// <summary>
        /// Transceiver name, for logging and debugging.
        /// </summary>
        public string Name;

        /// <summary>
        /// Initial value of <see cref="Transceiver.DesiredDirection"/>.
        /// </summary>
        public Transceiver.Direction InitialDesiredDirection = Transceiver.Direction.SendReceive;

        /// <summary>
        /// List of stream IDs to associate the transceiver with.
        /// </summary>
        public List<string> StreamIDs;
    }

    /// <summary>
    /// Settings to create a new audio transceiver wrapper.
    /// </summary>
    public class AudioTransceiverInitSettings : TransceiverInitSettings
    {

    }

    /// <summary>
    /// Settings to create a new video transceiver wrapper.
    /// </summary>
    public class VideoTransceiverInitSettings : TransceiverInitSettings
    {

    }

    /// <summary>
    /// The WebRTC peer connection object is the entry point to using WebRTC.
    /// </summary>
    public class PeerConnection : IDisposable
    {
        /// <summary>
        /// Delegate for <see cref="AudioTransceiverAdded"/> event.
        /// </summary>
        /// <param name="transceiver">The newly added audio transceiver.</param>
        public delegate void AudioTransceiverAddedDelegate(AudioTransceiver transceiver);

        /// <summary>
        /// Delegate for <see cref="VideoTransceiverAdded"/> event.
        /// </summary>
        /// <param name="transceiver">The newly added video transceiver.</param>
        public delegate void VideoTransceiverAddedDelegate(VideoTransceiver transceiver);

        /// <summary>
        /// Delegate for <see cref="AudioTrackAdded"/> event.
        /// </summary>
        /// <param name="track">The newly added audio track.</param>
        public delegate void AudioTrackAddedDelegate(RemoteAudioTrack track);

        /// <summary>
        /// Delegate for <see cref="AudioTrackRemoved"/> event.
        /// </summary>
        /// <param name="track">The audio track just removed.</param>
        public delegate void AudioTrackRemovedDelegate(AudioTransceiver transceiver, RemoteAudioTrack track);

        /// <summary>
        /// Delegate for <see cref="VideoTrackAdded"/> event.
        /// </summary>
        /// <param name="track">The newly added video track.</param>
        public delegate void VideoTrackAddedDelegate(RemoteVideoTrack track);

        /// <summary>
        /// Delegate for <see cref="VideoTrackRemoved"/> event.
        /// </summary>
        /// <param name="track">The video track just removed.</param>
        public delegate void VideoTrackRemovedDelegate(VideoTransceiver transceiver, RemoteVideoTrack track);

        /// <summary>
        /// Delegate for <see cref="DataChannelAdded"/> event.
        /// </summary>
        /// <param name="channel">The newly added data channel.</param>
        public delegate void DataChannelAddedDelegate(DataChannel channel);

        /// <summary>
        /// Delegate for <see cref="DataChannelRemoved"/> event.
        /// </summary>
        /// <param name="channel">The data channel just removed.</param>
        public delegate void DataChannelRemovedDelegate(DataChannel channel);

        /// <summary>
        /// Delegate for <see cref="LocalSdpReadytoSend"/> event.
        /// </summary>
        /// <param name="type">SDP message type, one of "offer", "answer", or "ice".</param>
        /// <param name="sdp">Raw SDP message content.</param>
        public delegate void LocalSdpReadyToSendDelegate(string type, string sdp);

        /// <summary>
        /// Delegate for the <see cref="IceCandidateReadytoSend"/> event.
        /// </summary>
        /// <param name="candidate">Raw SDP message describing the ICE candidate.</param>
        /// <param name="sdpMlineindex">Index of the m= line.</param>
        /// <param name="sdpMid">Media identifier</param>
        public delegate void IceCandidateReadytoSendDelegate(string candidate, int sdpMlineindex, string sdpMid);

        /// <summary>
        /// Delegate for the <see cref="IceStateChanged"/> event.
        /// </summary>
        /// <param name="newState">The new ICE connection state.</param>
        public delegate void IceStateChangedDelegate(IceConnectionState newState);

        /// <summary>
        /// Delegate for the <see cref="IceGatheringStateChanged"/> event.
        /// </summary>
        /// <param name="newState">The new ICE gathering state.</param>
        public delegate void IceGatheringStateChangedDelegate(IceGatheringState newState);

        /// <summary>
        /// Kind of WebRTC track.
        /// </summary>
        public enum TrackKind : uint
        {
            /// <summary>
            /// Unknown track kind. Generally not initialized or error.
            /// </summary>
            Unknown = 0,

            /// <summary>
            /// Audio track.
            /// </summary>
            Audio = 1,

            /// <summary>
            /// Video track.
            /// </summary>
            Video = 2,

            /// <summary>
            /// Data track.
            /// </summary>
            Data = 3
        };


        #region Codec filtering

        /// <summary>
        /// Name of the preferred audio codec, or empty to let WebRTC decide.
        /// See https://en.wikipedia.org/wiki/RTP_audio_video_profile for the standard SDP names.
        /// </summary>
        public string PreferredAudioCodec = string.Empty;

        /// <summary>
        /// Advanced use only. List of additional codec-specific arguments requested to the
        /// remote endpoint.
        /// </summary>
        /// <remarks>
        /// This must be a semicolon-separated list of "key=value" pairs. Arguments are passed as is,
        /// and there is no check on the validity of the parameter names nor their value.
        /// Arguments are added to the audio codec section of SDP messages sent to the remote endpoint.
        ///
        /// This is ignored if <see cref="PreferredAudioCodec"/> is an empty string, or is not
        /// a valid codec name found in the SDP message offer.
        /// </remarks>
        public string PreferredAudioCodecExtraParamsRemote = string.Empty;

        /// <summary>
        /// Advanced use only. List of additional codec-specific arguments set on the local endpoint.
        /// </summary>
        /// <remarks>
        /// This must be a semicolon-separated list of "key=value" pairs. Arguments are passed as is,
        /// and there is no check on the validity of the parameter names nor their value.
        /// Arguments are set locally by adding them to the audio codec section of SDP messages
        /// received from the remote endpoint.
        ///
        /// This is ignored if <see cref="PreferredAudioCodec"/> is an empty string, or is not
        /// a valid codec name found in the SDP message offer.
        /// </remarks>
        public string PreferredAudioCodecExtraParamsLocal = string.Empty;

        /// <summary>
        /// Name of the preferred video codec, or empty to let WebRTC decide.
        /// See https://en.wikipedia.org/wiki/RTP_audio_video_profile for the standard SDP names.
        /// </summary>
        public string PreferredVideoCodec = string.Empty;

        /// <summary>
        /// Advanced use only. List of additional codec-specific arguments requested to the
        /// remote endpoint.
        /// </summary>
        /// <remarks>
        /// This must be a semicolon-separated list of "key=value" pairs. Arguments are passed as is,
        /// and there is no check on the validity of the parameter names nor their value.
        /// Arguments are added to the video codec section of SDP messages sent to the remote endpoint.
        ///
        /// This is ignored if <see cref="PreferredVideoCodec"/> is an empty string, or is not
        /// a valid codec name found in the SDP message offer.
        /// </remarks>
        public string PreferredVideoCodecExtraParamsRemote = string.Empty;

        /// <summary>
        /// Advanced use only. List of additional codec-specific arguments set on the local endpoint.
        /// </summary>
        /// <remarks>
        /// This must be a semicolon-separated list of "key=value" pairs. Arguments are passed as is,
        /// and there is no check on the validity of the parameter names nor their value.
        /// Arguments are set locally by adding them to the video codec section of SDP messages
        /// received from the remote endpoint.
        ///
        /// This is ignored if <see cref="PreferredVideoCodec"/> is an empty string, or is not
        /// a valid codec name found in the SDP message offer.
        /// </remarks>
        public string PreferredVideoCodecExtraParamsLocal = string.Empty;

        #endregion


        /// <summary>
        /// A name for the peer connection, used for logging and debugging.
        /// </summary>
        public string Name { get; set; } = string.Empty;

        /// <summary>
        /// Boolean property indicating whether the peer connection has been initialized.
        /// </summary>
        public bool Initialized
        {
            get
            {
                lock (_openCloseLock)
                {
                    return (_initTask != null);
                }
            }
        }

        /// <summary>
        /// Indicates whether the peer connection is established and can exchange some
        /// track content (audio/video/data) with the remote peer.
        /// </summary>
        /// <remarks>
        /// This does not indicate whether the ICE exchange is done, as it
        /// may continue after the peer connection negotiated a first session.
        /// For ICE connection status, see the <see cref="IceStateChanged"/> event.
        /// </remarks>
        public bool IsConnected { get; private set; } = false;

        /// <summary>
        /// Collection of transceivers for the peer conneciton.
        /// Transceivers are present in the list in order of their media line index
        /// (<see cref="Transceiver.MlineIndex"/>). Once a transceiver is added to the
        /// peer connection, it cannot be removed, but its tracks can be changed.
        /// This requires some renegotiation.
        /// </summary>
        public List<Transceiver> Transceivers { get; } = new List<Transceiver>();

        /// <summary>
        /// Collection of audio transceivers of the peer connection.
        /// Once a transceiver is added to the peer connection, it cannot be removed,
        /// but its tracks can be changed. This requires some renegotiation.
        /// </summary>
        public IEnumerable<AudioTransceiver> AudioTransceivers
        {
            get
            {
                int num = Transceivers.Count;
                for (int i = 0; i < num; ++i)
                {
                    var tr = Transceivers[i];
                    if (tr.MediaKind == MediaKind.Audio)
                    {
                        yield return (AudioTransceiver)tr;
                    }
                }
            }
        }

        /// <summary>
        /// Collection of video transceivers of the peer connection.
        /// Once a transceiver is added to the peer connection, it cannot be removed,
        /// but its tracks can be changed. This requires some renegotiation.
        /// </summary>
        public IEnumerable<VideoTransceiver> VideoTransceivers
        {
            get
            {
                int num = Transceivers.Count;
                for (int i = 0; i < num; ++i)
                {
                    var tr = Transceivers[i];
                    if (tr.MediaKind == MediaKind.Video)
                    {
                        yield return (VideoTransceiver)tr;
                    }
                }
            }
        }

        /// <summary>
        /// Collection of local audio tracks attached to the peer connection.
        /// </summary>
        public List<LocalAudioTrack> LocalAudioTracks { get; } = new List<LocalAudioTrack>();

        /// <summary>
        /// Collection of local video tracks attached to the peer connection.
        /// </summary>
        public List<LocalVideoTrack> LocalVideoTracks { get; } = new List<LocalVideoTrack>();

        /// <summary>
        /// Collection of remote audio tracks attached to the peer connection.
        /// </summary>
        public List<RemoteAudioTrack> RemoteAudioTracks { get; } = new List<RemoteAudioTrack>();

        /// <summary>
        /// Collection of remote video tracks attached to the peer connection.
        /// </summary>
        public List<RemoteVideoTrack> RemoteVideoTracks { get; } = new List<RemoteVideoTrack>();

        /// <summary>
        /// Event fired when a connection is established.
        /// </summary>
        public event Action Connected;

        /// <summary>
        /// Event fired when a data channel is added to the peer connection.
        /// This event is always fired, whether the data channel is created by the local peer
        /// or the remote peer, and is negotiated (out-of-band) or not (in-band).
        /// If an in-band data channel is created by the local peer, the <see cref="DataChannel.ID"/>
        /// field is not yet available when this event is fired, because the ID has not been
        /// agreed upon with the remote peer yet.
        /// </summary>
        public event DataChannelAddedDelegate DataChannelAdded;

        /// <summary>
        /// Event fired when a data channel is removed from the peer connection.
        /// This event is always fired, whatever its creation method (negotiated or not)
        /// and original creator (local or remote peer).
        /// </summary>
        public event DataChannelRemovedDelegate DataChannelRemoved;

        /// <summary>
        /// Event that occurs when a local SDP message is ready to be transmitted.
        /// </summary>
        public event LocalSdpReadyToSendDelegate LocalSdpReadytoSend;

        /// <summary>
        /// Event that occurs when a local ICE candidate is ready to be transmitted.
        /// </summary>
        public event IceCandidateReadytoSendDelegate IceCandidateReadytoSend;

        /// <summary>
        /// Event that occurs when the state of the ICE connection changed.
        /// </summary>
        public event IceStateChangedDelegate IceStateChanged;

        /// <summary>
        /// Event that occurs when the state of the ICE gathering changed.
        /// </summary>
        public event IceGatheringStateChangedDelegate IceGatheringStateChanged;

        /// <summary>
        /// Event that occurs when a renegotiation of the session is needed.
        /// This generally occurs as a result of adding or removing tracks,
        /// and the user should call <see cref="CreateOffer"/> to actually
        /// start a renegotiation.
        /// </summary>
        public event Action RenegotiationNeeded;

        /// <summary>
        /// Event that occurs when an audio transceiver is added to the peer connection, either
        /// manually using <see cref="AddAudioTransceiver(AudioTransceiverInitSettings)"/>, or
        /// automatically as a result of a new session negotiation.
        /// </summary>
        /// <remarks>
        /// Transceivers cannot be removed from the peer connection, so there is no
        /// <c>AudioTransceiverRemoved</c> event.
        /// </remarks>
        public event AudioTransceiverAddedDelegate AudioTransceiverAdded;

        /// <summary>
        /// Event that occurs when a video transceiver is added to the peer connection, either
        /// manually using <see cref="AddVideoTransceiver(VideoTransceiverInitSettings)"/>, or
        /// automatically as a result of a new session negotiation.
        /// </summary>
        /// <remarks>
        /// Transceivers cannot be removed from the peer connection, so there is no
        /// <c>VideoTransceiverRemoved</c> event.
        /// </remarks>
        public event VideoTransceiverAddedDelegate VideoTransceiverAdded;

        /// <summary>
        /// Event that occurs when a remote audio track is added to the current connection.
        /// </summary>
        public event AudioTrackAddedDelegate AudioTrackAdded;

        /// <summary>
        /// Event that occurs when a remote audio track is removed from the current connection.
        /// </summary>
        public event AudioTrackRemovedDelegate AudioTrackRemoved;

        /// <summary>
        /// Event that occurs when a remote video track is added to the current connection.
        /// </summary>
        public event VideoTrackAddedDelegate VideoTrackAdded;

        /// <summary>
        /// Event that occurs when a remote video track is removed from the current connection.
        /// </summary>
        public event VideoTrackRemovedDelegate VideoTrackRemoved;

        /// <summary>
        /// GCHandle to self for the various native callbacks.
        /// This also acts as a marker of a connection created or in the process of being created.
        /// </summary>
        private GCHandle _selfHandle;

        /// <summary>
        /// Handle to the native PeerConnection object.
        /// </summary>
        /// <remarks>
        /// In native land this is a <code>Microsoft::MixedReality::WebRTC::PeerConnectionHandle</code>.
        /// </remarks>
        private PeerConnectionHandle _nativePeerhandle = new PeerConnectionHandle();

        /// <summary>
        /// Initialization task returned by <see cref="InitializeAsync"/>.
        /// </summary>
        private Task _initTask = null;

        /// <summary>
        /// Boolean to indicate if <see cref="Close"/> has been called and is waiting for a pending
        /// initializing task <see cref="_initTask"/> to complete or cancel.
        /// </summary>
        private bool _isClosing = false;

        /// <summary>
        /// Lock for asynchronous opening and closing of the connection, protecting
        /// changes to <see cref="_nativePeerhandle"/>, <see cref="_selfHandle"/>,
        /// <see cref="_initTask"/>, and <see cref="_isClosing"/>.
        /// </summary>
        private object _openCloseLock = new object();

        private PeerConnectionInterop.InteropCallbacks _interopCallbacks;
        private PeerConnectionInterop.PeerCallbackArgs _peerCallbackArgs;

        /// <summary>
        /// Lock for accessing the collections of tracks:
        /// - <see cref="LocalAudioTracks"/>
        /// - <see cref="LocalVideoTracks"/>
        /// - <see cref="RemoteAudioTracks"/>
        /// - <see cref="RemoteVideoTracks"/>
        /// </summary>
        private object _tracksLock = new object();


        #region Initializing and shutdown

        /// <summary>
        /// Create a new peer connection object. The object is initially created empty, and cannot be used
        /// until <see cref="InitializeAsync(PeerConnectionConfiguration, CancellationToken)"/> has completed
        /// successfully.
        /// </summary>
        public PeerConnection()
        {
            MainEventSource.Log.Initialize();
        }

        /// <summary>
        /// Initialize the current peer connection object asynchronously.
        ///
        /// Most other methods will fail unless this call completes successfully, as it initializes the
        /// underlying native implementation object required to create and manipulate the peer connection.
        ///
        /// Once this call asynchronously completed, the <see cref="Initialized"/> property becomes <c>true</c>.
        /// </summary>
        /// <param name="config">Configuration for initializing the peer connection.</param>
        /// <param name="token">Optional cancellation token for the initialize task. This is only used if
        /// the singleton task was created by this call, and not a prior call.</param>
        /// <returns>The singleton task used to initialize the underlying native peer connection.</returns>
        /// <remarks>This method is multi-thread safe, and will always return the same task object
        /// from the first call to it until the peer connection object is deinitialized. This allows
        /// multiple callers to all execute some action following the initialization, without the need
        /// to force a single caller and to synchronize with it.</remarks>
        public Task InitializeAsync(PeerConnectionConfiguration config = null, CancellationToken token = default)
        {
            lock (_openCloseLock)
            {
                // If Close() is waiting for _initTask to finish, do not return it.
                if (_isClosing)
                {
                    throw new OperationCanceledException("A closing operation is pending.");
                }

                // If _initTask has already been created, return it.
                if (_initTask != null)
                {
                    return _initTask;
                }

                // Allocate a GC handle to self for static P/Invoke callbacks to be able to call
                // back into methods of this peer connection object. The handle is released when
                // the peer connection is closed, to allow this object to be destroyed.
                Debug.Assert(!_selfHandle.IsAllocated);
                _selfHandle = GCHandle.Alloc(this, GCHandleType.Normal);

                // Create and lock in memory delegates for all the static callback wrappers (see below).
                // This avoids delegates being garbage-collected, since the P/Invoke mechanism by itself
                // does not guarantee their lifetime.
                _interopCallbacks = new PeerConnectionInterop.InteropCallbacks()
                {
                    Peer = this,
                    AudioTransceiverCreateObjectCallback = AudioTransceiverInterop.AudioTransceiverCreateObjectCallback,
                    AudioTransceiverFinishCreateCallbak = AudioTransceiverInterop.AudioTransceiverFinishCreateCallback,
                    VideoTransceiverCreateObjectCallback = VideoTransceiverInterop.VideoTransceiverCreateObjectCallback,
                    VideoTransceiverFinishCreateCallbak = VideoTransceiverInterop.VideoTransceiverFinishCreateCallback,
                    RemoteAudioTrackCreateObjectCallback = RemoteAudioTrackInterop.RemoteAudioTrackCreateObjectCallback,
                    RemoteVideoTrackCreateObjectCallback = RemoteVideoTrackInterop.RemoteVideoTrackCreateObjectCallback,
                    DataChannelCreateObjectCallback = DataChannelInterop.DataChannelCreateObjectCallback,
                };
                _peerCallbackArgs = new PeerConnectionInterop.PeerCallbackArgs()
                {
                    Peer = this,
                    DataChannelAddedCallback = PeerConnectionInterop.DataChannelAddedCallback,
                    DataChannelRemovedCallback = PeerConnectionInterop.DataChannelRemovedCallback,
                    ConnectedCallback = PeerConnectionInterop.ConnectedCallback,
                    LocalSdpReadytoSendCallback = PeerConnectionInterop.LocalSdpReadytoSendCallback,
                    IceCandidateReadytoSendCallback = PeerConnectionInterop.IceCandidateReadytoSendCallback,
                    IceStateChangedCallback = PeerConnectionInterop.IceStateChangedCallback,
                    IceGatheringStateChangedCallback = PeerConnectionInterop.IceGatheringStateChangedCallback,
                    RenegotiationNeededCallback = PeerConnectionInterop.RenegotiationNeededCallback,
                    AudioTrackAddedCallback = PeerConnectionInterop.AudioTrackAddedCallback,
                    AudioTrackRemovedCallback = PeerConnectionInterop.AudioTrackRemovedCallback,
                    VideoTrackAddedCallback = PeerConnectionInterop.VideoTrackAddedCallback,
                    VideoTrackRemovedCallback = PeerConnectionInterop.VideoTrackRemovedCallback,
                };

                // Cache values in local variables before starting async task, to avoid any
                // subsequent external change from affecting that task.
                // Also set default values, as the native call doesn't handle NULL.
                PeerConnectionInterop.PeerConnectionConfiguration nativeConfig;
                if (config != null)
                {
                    nativeConfig = new PeerConnectionInterop.PeerConnectionConfiguration
                    {
                        EncodedIceServers = string.Join("\n\n", config.IceServers),
                        IceTransportType = config.IceTransportType,
                        BundlePolicy = config.BundlePolicy,
                        SdpSemantic = config.SdpSemantic,
                    };
                }
                else
                {
                    nativeConfig = new PeerConnectionInterop.PeerConnectionConfiguration();
                }

                // On UWP PeerConnectionCreate() fails on main UI thread, so always initialize the native peer
                // connection asynchronously from a background worker thread.
                _initTask = Task.Run(() =>
                {
                    token.ThrowIfCancellationRequested();

                    uint res = PeerConnectionInterop.PeerConnection_Create(nativeConfig, GCHandle.ToIntPtr(_selfHandle), out _nativePeerhandle);

                    lock (_openCloseLock)
                    {
                        // Handle errors
                        if ((res != Utils.MRS_SUCCESS) || _nativePeerhandle.IsInvalid)
                        {
                            if (_selfHandle.IsAllocated)
                            {
                                _interopCallbacks = null;
                                _peerCallbackArgs = null;
                                _selfHandle.Free();
                            }

                            Utils.ThrowOnErrorCode(res);
                            throw new Exception(); // if res == MRS_SUCCESS but handle is NULL
                        }

                        // The connection may have been aborted while being created, either via the
                        // cancellation token, or by calling Close() after the synchronous codepath
                        // above but before this task had a chance to run in the background.
                        if (token.IsCancellationRequested)
                        {
                            // Cancelled by token
                            _nativePeerhandle.Close();
                            throw new OperationCanceledException(token);
                        }
                        if (!_selfHandle.IsAllocated)
                        {
                            // Cancelled by calling Close()
                            _nativePeerhandle.Close();
                            throw new OperationCanceledException();
                        }

                        // Register all trampoline callbacks. Note that even passing a static managed method
                        // for the callback is not safe, because the compiler implicitly creates a delegate
                        // object (a static method is not a delegate itself; it can be wrapped inside one),
                        // and that delegate object will be garbage collected immediately at the end of this
                        // block. Instead, a delegate needs to be explicitly created and locked in memory.
                        // Since the current PeerConnection instance is already locked via _selfHandle,
                        // and it references all delegates via _peerCallbackArgs, those also can't be GC'd.
                        var self = GCHandle.ToIntPtr(_selfHandle);
                        var interopCallbacks = new PeerConnectionInterop.MarshaledInteropCallbacks
                        {
                            AudioTransceiverCreateObjectCallback = _interopCallbacks.AudioTransceiverCreateObjectCallback,
                            AudioTransceiverFinishCreateCallbak = _interopCallbacks.AudioTransceiverFinishCreateCallbak,
                            VideoTransceiverCreateObjectCallback = _interopCallbacks.VideoTransceiverCreateObjectCallback,
                            VideoTransceiverFinishCreateCallbak = _interopCallbacks.VideoTransceiverFinishCreateCallbak,
                            RemoteAudioTrackCreateObjectCallback = _interopCallbacks.RemoteAudioTrackCreateObjectCallback,
                            RemoteVideoTrackCreateObjectCallback = _interopCallbacks.RemoteVideoTrackCreateObjectCallback,
                            DataChannelCreateObjectCallback = _interopCallbacks.DataChannelCreateObjectCallback
                        };
                        PeerConnectionInterop.PeerConnection_RegisterInteropCallbacks(
                            _nativePeerhandle, in interopCallbacks);
                        PeerConnectionInterop.PeerConnection_RegisterConnectedCallback(
                            _nativePeerhandle, _peerCallbackArgs.ConnectedCallback, self);
                        PeerConnectionInterop.PeerConnection_RegisterLocalSdpReadytoSendCallback(
                            _nativePeerhandle, _peerCallbackArgs.LocalSdpReadytoSendCallback, self);
                        PeerConnectionInterop.PeerConnection_RegisterIceCandidateReadytoSendCallback(
                            _nativePeerhandle, _peerCallbackArgs.IceCandidateReadytoSendCallback, self);
                        PeerConnectionInterop.PeerConnection_RegisterIceStateChangedCallback(
                            _nativePeerhandle, _peerCallbackArgs.IceStateChangedCallback, self);
                        PeerConnectionInterop.PeerConnection_RegisterIceGatheringStateChangedCallback(
                            _nativePeerhandle, _peerCallbackArgs.IceGatheringStateChangedCallback, self);
                        PeerConnectionInterop.PeerConnection_RegisterRenegotiationNeededCallback(
                            _nativePeerhandle, _peerCallbackArgs.RenegotiationNeededCallback, self);
                        PeerConnectionInterop.PeerConnection_RegisterAudioTrackAddedCallback(
                            _nativePeerhandle, _peerCallbackArgs.AudioTrackAddedCallback, self);
                        PeerConnectionInterop.PeerConnection_RegisterAudioTrackRemovedCallback(
                            _nativePeerhandle, _peerCallbackArgs.AudioTrackRemovedCallback, self);
                        PeerConnectionInterop.PeerConnection_RegisterVideoTrackAddedCallback(
                            _nativePeerhandle, _peerCallbackArgs.VideoTrackAddedCallback, self);
                        PeerConnectionInterop.PeerConnection_RegisterVideoTrackRemovedCallback(
                            _nativePeerhandle, _peerCallbackArgs.VideoTrackRemovedCallback, self);
                        PeerConnectionInterop.PeerConnection_RegisterDataChannelAddedCallback(
                            _nativePeerhandle, _peerCallbackArgs.DataChannelAddedCallback, self);
                        PeerConnectionInterop.PeerConnection_RegisterDataChannelRemovedCallback(
                            _nativePeerhandle, _peerCallbackArgs.DataChannelRemovedCallback, self);
                    }
                }, token);

                return _initTask;
            }
        }

        /// <summary>
        /// Close the peer connection and destroy the underlying native resources.
        /// </summary>
        /// <remarks>This is equivalent to <see cref="Dispose"/>.</remarks>
        /// <seealso cref="Dispose"/>
        public void Close()
        {
            // Begin shutdown sequence
            Task initTask = null;
            lock (_openCloseLock)
            {
                // If the connection is not initialized, return immediately.
                if (_initTask == null)
                {
                    return;
                }

                // Indicate to InitializeAsync() that it should stop returning _initTask
                // or create a new instance of it, even if it is NULL.
                _isClosing = true;

                // Ensure both Initialized and IsConnected return false
                initTask = _initTask;
                _initTask = null; // This marks the Initialized property as false
                IsConnected = false;

                // Unregister all callbacks and free the delegates
                var interopCallbacks = new PeerConnectionInterop.MarshaledInteropCallbacks();
                PeerConnectionInterop.PeerConnection_RegisterInteropCallbacks(
                    _nativePeerhandle, in interopCallbacks);
                PeerConnectionInterop.PeerConnection_RegisterConnectedCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                PeerConnectionInterop.PeerConnection_RegisterLocalSdpReadytoSendCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                PeerConnectionInterop.PeerConnection_RegisterIceCandidateReadytoSendCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                PeerConnectionInterop.PeerConnection_RegisterIceStateChangedCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                PeerConnectionInterop.PeerConnection_RegisterIceGatheringStateChangedCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                PeerConnectionInterop.PeerConnection_RegisterRenegotiationNeededCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                PeerConnectionInterop.PeerConnection_RegisterAudioTrackAddedCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                PeerConnectionInterop.PeerConnection_RegisterAudioTrackRemovedCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                PeerConnectionInterop.PeerConnection_RegisterVideoTrackAddedCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                PeerConnectionInterop.PeerConnection_RegisterVideoTrackRemovedCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                PeerConnectionInterop.PeerConnection_RegisterDataChannelAddedCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                PeerConnectionInterop.PeerConnection_RegisterDataChannelRemovedCallback(
                    _nativePeerhandle, null, IntPtr.Zero);
                if (_selfHandle.IsAllocated)
                {
                    _interopCallbacks = null;
                    _peerCallbackArgs = null;
                    _selfHandle.Free();
                }
            }

            // Wait for any pending initializing to finish.
            // This must be outside of the lock because the initialization task will
            // eventually need to acquire the lock to complete.
            initTask.Wait();

            // Close the native peer connection, disconnecting from the remote peer if currently connected.
            PeerConnectionInterop.PeerConnection_Close(_nativePeerhandle);

            // Destroy the native peer connection object. This may be delayed if a P/Invoke callback is underway,
            // but will be handled at some point anyway, even if the PeerConnection managed instance is gone.
            _nativePeerhandle.Close();

            // Notify local tracks they have been removed
            int count = LocalAudioTracks.Count;
            while (count > 0)
            {
                LocalAudioTracks[count - 1].OnTrackRemoved(this);
                --count;
            }
            Debug.Assert(LocalAudioTracks.Count == 0);
            count = LocalVideoTracks.Count;
            while (count > 0)
            {
                LocalVideoTracks[count - 1].OnTrackRemoved(this);
                --count;
            }
            Debug.Assert(LocalVideoTracks.Count == 0);
            count = RemoteAudioTracks.Count;
            while (count > 0)
            {
                var track = RemoteAudioTracks[count - 1];
                track.OnTrackRemoved(this);
                track.Dispose(); // remote tracks are owned
                --count;
            }
            Debug.Assert(RemoteAudioTracks.Count == 0);
            count = RemoteVideoTracks.Count;
            while (count > 0)
            {
                var track = RemoteVideoTracks[count - 1];
                track.OnTrackRemoved(this);
                track.Dispose(); // remote tracks are owned
                --count;
            }
            Debug.Assert(RemoteVideoTracks.Count == 0);

            // Dispose of owned objects
            lock (_tracksLock)
            {
                foreach (var transceiver in Transceivers)
                {
                    if (transceiver == null)
                    {
                        continue;
                    }
                    if (transceiver.MediaKind == MediaKind.Audio)
                    {
                        ((AudioTransceiver)transceiver)._nativeHandle.Close();
                    }
                    else
                    {
                        ((VideoTransceiver)transceiver)._nativeHandle.Close();
                    }
                }
                Transceivers.Clear();
            }

            // Complete shutdown sequence and re-enable InitializeAsync()
            lock (_openCloseLock)
            {
                _isClosing = false;
            }
        }

        /// <summary>
        /// Dispose of native resources by closing the peer connection.
        /// </summary>
        /// <remarks>This is equivalent to <see cref="Close"/>.</remarks>
        /// <seealso cref="Close"/>
        public void Dispose() => Close();

        #endregion


        #region Transceivers

        /// <summary>
        /// Add to the current connection a new audio transceiver.
        /// 
        /// A transceiver is a container for a pair of audio tracks, one local sending to the remote
        /// peer, and one remote receiving from the remote peer. Both are optional, and the transceiver
        /// can be in receive-only mode (no local track), in send-only mode (no remote track), or
        /// inactive (neither local nor remote track).
        /// 
        /// Once a transceiver is added to the peer connection, it cannot be removed, but its tracks can be
        /// changed (this requires some renegotiation).
        /// </summary>
        /// <param name="settings"></param>
        /// <returns>The newly created transceiver.</returns>
        public AudioTransceiver AddAudioTransceiver(AudioTransceiverInitSettings settings = null)
        {
            ThrowIfConnectionNotOpen();
            settings = settings ?? new AudioTransceiverInitSettings();
            int mlineIndex = Transceivers.Count; //< TODO: retrieve from interop for robustness?
            var transceiver = new AudioTransceiver(this, mlineIndex, settings.Name, settings.InitialDesiredDirection);
            var config = new AudioTransceiverInterop.InitConfig(transceiver, settings);
            Debug.Assert(transceiver.DesiredDirection == config.desiredDirection);
            uint res = PeerConnectionInterop.PeerConnection_AddAudioTransceiver(_nativePeerhandle, in config,
                out AudioTransceiverHandle transceiverHandle);
            Utils.ThrowOnErrorCode(res);
            transceiver.SetHandle(transceiverHandle);
            OnTransceiverAdded(transceiver);
            return transceiver;
        }

        /// <summary>
        /// Add to the current connection a new video transceiver.
        /// 
        /// A transceiver is a container for a pair of video tracks, one local sending to the remote
        /// peer, and one remote receiving from the remote peer. Both are optional, and the transceiver
        /// can be in receive-only mode (no local track), in send-only mode (no remote track), or
        /// inactive (neither local nor remote track).
        /// 
        /// Once a transceiver is added to the peer connection, it cannot be removed, but its tracks can be
        /// changed (this requires some renegotiation).
        /// </summary>
        /// <param name="settings"></param>
        /// <returns>The newly created transceiver.</returns>
        public VideoTransceiver AddVideoTransceiver(VideoTransceiverInitSettings settings = null)
        {
            ThrowIfConnectionNotOpen();
            settings = settings ?? new VideoTransceiverInitSettings();
            int mlineIndex = Transceivers.Count; //< TODO: retrieve from interop for robustness?
            var transceiver = new VideoTransceiver(this, mlineIndex, settings.Name, settings.InitialDesiredDirection);
            var config = new VideoTransceiverInterop.InitConfig(transceiver, settings);
            Debug.Assert(transceiver.DesiredDirection == config.desiredDirection);
            uint res = PeerConnectionInterop.PeerConnection_AddVideoTransceiver(_nativePeerhandle, in config,
                out VideoTransceiverHandle transceiverHandle);
            Utils.ThrowOnErrorCode(res);
            transceiver.SetHandle(transceiverHandle);
            OnTransceiverAdded(transceiver);
            return transceiver;
        }

        #endregion


        #region Data tracks

        /// <summary>
        /// Add a new out-of-band data channel with the given ID.
        ///
        /// A data channel is negotiated out-of-band when the peers agree on an identifier by any mean
        /// not known to WebRTC, and both open a data channel with that ID. The WebRTC will match the
        /// incoming and outgoing pipes by this ID to allow sending and receiving through that channel.
        ///
        /// This requires some external mechanism to agree on an available identifier not otherwise taken
        /// by another channel, and also requires to ensure that both peers explicitly open that channel.
        /// </summary>
        /// <param name="id">The unique data channel identifier to use.</param>
        /// <param name="label">The data channel name.</param>
        /// <param name="ordered">Indicates whether data channel messages are ordered (see
        /// <see cref="DataChannel.Ordered"/>).</param>
        /// <param name="reliable">Indicates whether data channel messages are reliably delivered
        /// (see <see cref="DataChannel.Reliable"/>).</param>
        /// <returns>Returns a task which completes once the data channel is created.</returns>
        /// <exception xref="InvalidOperationException">The peer connection is not initialized.</exception>
        /// <exception cref="SctpNotNegotiatedException">SCTP not negotiated. Call <see cref="CreateOffer()"/> first.</exception>
        /// <exception xref="ArgumentOutOfRangeException">Invalid data channel ID, must be in [0:65535].</exception>
        /// <remarks>
        /// Data channels use DTLS over SCTP, which ensure in particular that messages are encrypted. To that end,
        /// while establishing a connection with the remote peer, some specific SCTP handshake must occur. This
        /// handshake is only performed if at least one data channel was added to the peer connection when the
        /// connection starts its negotiation with <see cref="CreateOffer"/>. Therefore, if the user wants to use
        /// a data channel at any point during the lifetime of this peer connection, it is critical to add at least
        /// one data channel before <see cref="CreateOffer"/> is called. Otherwise all calls will fail with an
        /// <see cref="SctpNotNegotiatedException"/> exception.
        /// </remarks>
        public async Task<DataChannel> AddDataChannelAsync(ushort id, string label, bool ordered, bool reliable)
        {
            if (id < 0)
            {
                throw new ArgumentOutOfRangeException("id", id, "Data channel ID must be greater than or equal to zero.");
            }
            return await AddDataChannelAsyncImpl(id, label, ordered, reliable);
        }

        /// <summary>
        /// Add a new in-band data channel whose ID will be determined by the implementation.
        ///
        /// A data channel is negotiated in-band when one peer requests its creation to the WebRTC core,
        /// and the implementation negotiates with the remote peer an appropriate ID by sending some
        /// SDP offer message. In that case once accepted the other peer will automatically create the
        /// appropriate data channel on its side with that negotiated ID, and the ID will be returned on
        /// both sides to the user for information.
        ///
        /// Compared to out-of-band messages, this requires exchanging some SDP messages, but avoids having
        /// to determine a common unused ID and having to explicitly open the data channel on both sides.
        /// </summary>
        /// <param name="label">The data channel name.</param>
        /// <param name="ordered">Indicates whether data channel messages are ordered (see
        /// <see cref="DataChannel.Ordered"/>).</param>
        /// <param name="reliable">Indicates whether data channel messages are reliably delivered
        /// (see <see cref="DataChannel.Reliable"/>).</param>
        /// <returns>Returns a task which completes once the data channel is created.</returns>
        /// <exception xref="InvalidOperationException">The peer connection is not initialized.</exception>
        /// <exception cref="SctpNotNegotiatedException">SCTP not negotiated. Call <see cref="CreateOffer()"/> first.</exception>
        /// <exception xref="ArgumentOutOfRangeException">Invalid data channel ID, must be in [0:65535].</exception>
        /// <remarks>
        /// See the critical remark about SCTP handshake in <see cref="AddDataChannelAsync(ushort, string, bool, bool)"/>.
        /// </remarks>
        public async Task<DataChannel> AddDataChannelAsync(string label, bool ordered, bool reliable)
        {
            return await AddDataChannelAsyncImpl(-1, label, ordered, reliable);
        }

        /// <summary>
        /// Add a new in-band or out-of-band data channel.
        /// </summary>
        /// <param name="id">Identifier in [0:65535] of the out-of-band data channel, or <c>-1</c> for in-band.</param>
        /// <param name="label">The data channel name.</param>
        /// <param name="ordered">Indicates whether data channel messages are ordered (see
        /// <see cref="DataChannel.Ordered"/>).</param>
        /// <param name="reliable">Indicates whether data channel messages are reliably delivered
        /// (see <see cref="DataChannel.Reliable"/>).</param>
        /// <returns>Returns a task which completes once the data channel is created.</returns>
        /// <exception xref="InvalidOperationException">The peer connection is not initialized.</exception>
        /// <exception xref="InvalidOperationException">SCTP not negotiated.</exception>
        /// <exception xref="ArgumentOutOfRangeException">Invalid data channel ID, must be in [0:65535].</exception>
        private async Task<DataChannel> AddDataChannelAsyncImpl(int id, string label, bool ordered, bool reliable)
        {
            // Preconditions
            ThrowIfConnectionNotOpen();

            // Create the wrapper
            var config = new DataChannelInterop.CreateConfig
            {
                id = id,
                label = label,
                flags = (ordered ? 0x1u : 0x0u) | (reliable ? 0x2u : 0x0u)
            };
            DataChannelInterop.Callbacks callbacks;
            var dataChannel = DataChannelInterop.CreateWrapper(this, config, out callbacks);
            if (dataChannel == null)
            {
                return null;
            }

            // Create the native channel
            return await Task.Run(() =>
            {
                IntPtr nativeHandle = IntPtr.Zero;
                var wrapperGCHandle = GCHandle.Alloc(dataChannel, GCHandleType.Normal);
                var wrapperHandle = GCHandle.ToIntPtr(wrapperGCHandle);
                uint res = PeerConnectionInterop.PeerConnection_AddDataChannel(_nativePeerhandle, wrapperHandle, config, callbacks, ref nativeHandle);
                if (res == Utils.MRS_SUCCESS)
                {
                    DataChannelInterop.SetHandle(dataChannel, nativeHandle);
                    return dataChannel;
                }

                // Some error occurred, callbacks are not registered, so remove the GC lock.
                wrapperGCHandle.Free();
                dataChannel.Dispose();
                dataChannel = null;

                Utils.ThrowOnErrorCode(res);
                return null; // for the compiler
            });
        }

        internal bool RemoveDataChannel(IntPtr dataChannelHandle)
        {
            ThrowIfConnectionNotOpen();
            return (PeerConnectionInterop.PeerConnection_RemoveDataChannel(_nativePeerhandle, dataChannelHandle) == Utils.MRS_SUCCESS);
        }

        #endregion


        #region Signaling

        /// <summary>
        /// Inform the WebRTC peer connection of a newly received ICE candidate.
        /// </summary>
        /// <param name="sdpMid"></param>
        /// <param name="sdpMlineindex"></param>
        /// <param name="candidate"></param>
        /// <exception xref="InvalidOperationException">The peer connection is not initialized.</exception>
        public void AddIceCandidate(string sdpMid, int sdpMlineindex, string candidate)
        {
            MainEventSource.Log.AddIceCandidate(sdpMid, sdpMlineindex, candidate);
            ThrowIfConnectionNotOpen();
            PeerConnectionInterop.PeerConnection_AddIceCandidate(_nativePeerhandle, sdpMid, sdpMlineindex, candidate);
        }

        /// <summary>
        /// Create an SDP offer message as an attempt to establish a connection.
        /// Once the message is ready to be sent, the <see cref="LocalSdpReadytoSend"/> event is fired
        /// to allow the user to send that message to the remote peer via its selected signaling solution.
        /// </summary>
        /// <returns><c>true</c> if the offer creation task was successfully submitted.</returns>
        /// <exception xref="InvalidOperationException">The peer connection is not initialized.</exception>
        /// <remarks>
        /// The SDP offer message is not successfully created until the <see cref="LocalSdpReadytoSend"/>
        /// event is triggered, and may still fail even if this method returns <c>true</c>, for example if
        /// the peer connection is not in a valid state to create an offer.
        /// </remarks>
        public bool CreateOffer()
        {
            MainEventSource.Log.CreateOffer();
            ThrowIfConnectionNotOpen();
            return (PeerConnectionInterop.PeerConnection_CreateOffer(_nativePeerhandle) == Utils.MRS_SUCCESS);
        }

        /// <summary>
        /// Create an SDP answer message to a previously-received offer, to accept a connection.
        /// Once the message is ready to be sent, the <see cref="LocalSdpReadytoSend"/> event is fired
        /// to allow the user to send that message to the remote peer via its selected signaling solution.
        /// Note that this cannot be called before <see cref="SetRemoteDescriptionAsync(string, string)"/>
        /// successfully completed and applied the remote offer.
        /// </summary>
        /// <returns><c>true</c> if the answer creation task was successfully submitted.</returns>
        /// <exception xref="InvalidOperationException">The peer connection is not initialized.</exception>
        /// <remarks>
        /// The SDP answer message is not successfully created until the <see cref="LocalSdpReadytoSend"/>
        /// event is triggered, and may still fail even if this method returns <c>true</c>, for example if
        /// the peer connection is not in a valid state to create an answer.
        /// </remarks>
        public bool CreateAnswer()
        {
            MainEventSource.Log.CreateAnswer();
            ThrowIfConnectionNotOpen();
            return (PeerConnectionInterop.PeerConnection_CreateAnswer(_nativePeerhandle) == Utils.MRS_SUCCESS);
        }

        /// <summary>
        /// Set the bitrate allocated to all RTP streams sent by this connection.
        /// Other limitations might affect these limits and are respected (for example "b=AS" in SDP).
        /// </summary>
        /// <param name="minBitrateBps">Minimum bitrate in bits per second.</param>
        /// <param name="startBitrateBps">Start/current target bitrate in bits per second.</param>
        /// <param name="maxBitrateBps">Maximum bitrate in bits per second.</param>
        public void SetBitrate(uint? minBitrateBps = null, uint? startBitrateBps = null, uint? maxBitrateBps = null)
        {
            ThrowIfConnectionNotOpen();
            int signedMinBitrateBps = minBitrateBps.HasValue ? (int)minBitrateBps.Value : -1;
            int signedStartBitrateBps = startBitrateBps.HasValue ? (int)startBitrateBps.Value : -1;
            int signedMaxBitrateBps = maxBitrateBps.HasValue ? (int)maxBitrateBps.Value : -1;
            uint res = PeerConnectionInterop.PeerConnection_SetBitrate(_nativePeerhandle,
                signedMinBitrateBps, signedStartBitrateBps, signedMaxBitrateBps);
            Utils.ThrowOnErrorCode(res);
        }

        /// <summary>
        /// Pass the given SDP description received from the remote peer via signaling to the
        /// underlying WebRTC implementation, which will parse and use it.
        ///
        /// This must be called by the signaler when receiving a message. Once this operation
        /// has completed, it is safe to call <see cref="CreateAnswer"/>.
        /// </summary>
        /// <param name="type">The type of SDP message ("offer" or "answer")</param>
        /// <param name="sdp">The content of the SDP message</param>
        /// <returns>Returns a task which completes once the remote description has been applied and transceivers
        /// have been updated.</returns>
        /// <exception xref="InvalidOperationException">The peer connection is not initialized.</exception>
        public Task SetRemoteDescriptionAsync(string type, string sdp)
        {
            ThrowIfConnectionNotOpen();
            return PeerConnectionInterop.SetRemoteDescriptionAsync(_nativePeerhandle, type, sdp);
        }

        #endregion

        /// <summary>
        /// Subset of RTCDataChannelStats. See <see href="https://www.w3.org/TR/webrtc-stats/#dcstats-dict*"/>
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct DataChannelStats
        {
            /// <summary>
            /// Unix timestamp (time since Epoch) of the statistics. For remote statistics, this is
            /// the time at which the information reached the local endpoint.
            /// </summary>
            public long TimestampUs;

            /// <summary>
            /// <see cref="DataChannel.ID"/> of the data channel associated with these statistics.
            /// </summary>
            public long DataChannelIdentifier;

            /// <summary>
            /// Total number of API message event sent.
            /// </summary>
            public uint MessagesSent;

            /// <summary>
            /// Total number of payload bytes sent, excluding headers and paddings.
            /// </summary>
            public ulong BytesSent;

            /// <summary>
            /// Total number of API message events received.
            /// </summary>
            public uint MessagesReceived;

            /// <summary>
            /// Total number of payload bytes received, excluding headers and paddings.
            /// </summary>
            public ulong BytesReceived;
        }

        /// <summary>
        /// Subset of RTCMediaStreamTrack (audio sender) and RTCOutboundRTPStreamStats.
        /// See <see href="https://www.w3.org/TR/webrtc-stats/#raststats-dict*"/>
        /// and <see href="https://www.w3.org/TR/webrtc-stats/#sentrtpstats-dict*"/>.
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public unsafe struct AudioSenderStats
        {
            #region Track statistics

            /// <summary>
            /// Unix timestamp (time since Epoch) of the audio statistics. For remote statistics,
            /// this is the time at which the information reached the local endpoint.
            /// </summary>
            public long TrackStatsTimestampUs;

            /// <summary>
            /// Track identifier.
            /// </summary>
            [MarshalAs(UnmanagedType.LPStr)]
            public string TrackIdentifier;

            /// <summary>
            /// Linear audio level of the media source, in [0:1] range, averaged over a small interval.
            /// </summary>
            public double AudioLevel;

            /// <summary>
            /// Total audio energy of the media source. For multi-channel sources (stereo, etc.) this is
            /// the highest energy of any of the channels for each sample.
            /// </summary>
            public double TotalAudioEnergy;

            /// <summary>
            /// Total duration in seconds of all the samples produced by the media source for the lifetime
            /// of the underlying internal statistics object. Like <see cref="TotalAudioEnergy"/> this is not
            /// affected by the number of channels per sample.
            /// </summary>
            public double TotalSamplesDuration;

            #endregion


            #region RTP statistics

            /// <summary>
            /// Unix timestamp (time since Epoch) of the RTP statistics. For remote statistics,
            /// this is the time at which the information reached the local endpoint.
            /// </summary>
            public long RtpStatsTimestampUs;

            /// <summary>
            /// Total number of RTP packets sent for this SSRC.
            /// </summary>
            public uint PacketsSent;

            /// <summary>
            /// Total number of bytes sent for this SSRC.
            /// </summary>
            public ulong BytesSent;

            #endregion
        }

        /// <summary>
        /// Subset of RTCMediaStreamTrack (audio receiver) and RTCInboundRTPStreamStats.
        /// See <see href="https://www.w3.org/TR/webrtc-stats/#aststats-dict*"/>
        /// and <see href="https://www.w3.org/TR/webrtc-stats/#inboundrtpstats-dict*"/>.
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct AudioReceiverStats
        {
            #region Track statistics

            /// <summary>
            /// Unix timestamp (time since Epoch) of the statistics. For remote statistics, this is
            /// the time at which the information reached the local endpoint.
            /// </summary>
            public long TrackStatsTimestampUs;

            /// <summary>
            /// Track identifier.
            /// </summary>
            public string TrackIdentifier;

            /// <summary>
            /// Linear audio level of the receiving track, in [0:1] range, averaged over a small interval.
            /// </summary>
            public double AudioLevel;

            /// <summary>
            /// Total audio energy of the received track. For multi-channel sources (stereo, etc.) this is
            /// the highest energy of any of the channels for each sample.
            /// </summary>
            public double TotalAudioEnergy;

            /// <summary>
            /// Total number of RTP samples received for this audio stream.
            /// Like <see cref="TotalAudioEnergy"/> this is not affected by the number of channels per sample.
            /// </summary>
            public double TotalSamplesReceived;

            /// <summary>
            /// Total duration in seconds of all the samples received (and thus counted by <see cref="TotalSamplesReceived"/>).
            /// Like <see cref="TotalAudioEnergy"/> this is not affected by the number of channels per sample.
            /// </summary>
            public double TotalSamplesDuration;

            #endregion


            #region RTP statistics

            /// <summary>
            /// Unix timestamp (time since Epoch) of the RTP statistics. For remote statistics,
            /// this is the time at which the information reached the local endpoint.
            /// </summary>
            public long RtpStatsTimestampUs;

            /// <summary>
            /// Total number of RTP packets received for this SSRC.
            /// </summary>
            public uint PacketsReceived;

            /// <summary>
            /// Total number of bytes received for this SSRC.
            /// </summary>
            public ulong BytesReceived;

            #endregion
        }

        /// <summary>
        /// Subset of RTCMediaStreamTrack (video sender) and RTCOutboundRTPStreamStats.
        /// See <see href="https://www.w3.org/TR/webrtc-stats/#vsstats-dict*"/>
        /// and <see href="https://www.w3.org/TR/webrtc-stats/#sentrtpstats-dict*"/>.
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct VideoSenderStats
        {
            #region Track statistics

            /// <summary>
            /// Unix timestamp (time since Epoch) of the track statistics. For remote statistics,
            /// this is the time at which the information reached the local endpoint.
            /// </summary>
            public long TrackStatsTimestampUs;

            /// <summary>
            /// Track identifier.
            /// </summary>
            public string TrackIdentifier;

            /// <summary>
            /// Total number of frames sent on this RTP stream.
            /// </summary>
            public uint FramesSent;

            /// <summary>
            /// Total number of huge frames sent by this RTP stream. Huge frames are frames that have
            /// an encoded size at least 2.5 times the average size of the frames.
            /// </summary>
            public uint HugeFramesSent;

            #endregion


            #region RTP statistics

            /// <summary>
            /// Unix timestamp (time since Epoch) of the RTP statistics. For remote statistics,
            /// this is the time at which the information reached the local endpoint.
            /// </summary>
            public long RtpStatsTimestampUs;

            /// <summary>
            /// Total number of RTP packets sent for this SSRC.
            /// </summary>
            public uint PacketsSent;

            /// <summary>
            /// Total number of bytes sent for this SSRC.
            /// </summary>
            public ulong BytesSent;

            /// <summary>
            /// Total number of frames successfully encoded for this RTP media stream.
            /// </summary>
            public uint FramesEncoded;

            #endregion
        }

        /// <summary>
        /// Subset of RTCMediaStreamTrack (video receiver) + RTCInboundRTPStreamStats.
        /// See <see href="https://www.w3.org/TR/webrtc-stats/#rvststats-dict*"/>
        /// and <see href="https://www.w3.org/TR/webrtc-stats/#inboundrtpstats-dict*"/>
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct VideoReceiverStats
        {
            #region Track statistics

            /// <summary>
            /// Unix timestamp (time since Epoch) of the track statistics. For remote statistics,
            /// this is the time at which the information reached the local endpoint.
            /// </summary>
            public long TrackStatsTimestampUs;

            /// <summary>
            /// Track identifier.
            /// </summary>
            public string TrackIdentifier;

            /// <summary>
            /// Total number of complete frames received on this RTP stream.
            /// </summary>
            public uint FramesReceived;

            /// <summary>
            /// Total number since the receiver was created of frames dropped prior to decode or
            /// dropped because the frame missed its display deadline for this receiver's track.
            /// </summary>
            public uint FramesDropped;

            #endregion


            #region RTP statistics

            /// <summary>
            /// Unix timestamp (time since Epoch) of the RTP statistics. For remote statistics,
            /// this is the time at which the information reached the local endpoint.
            /// </summary>
            public long RtpStatsTimestampUs;

            /// <summary>
            /// Total number of RTP packets received for this SSRC.
            /// </summary>
            public uint PacketsReceived;

            /// <summary>
            /// Total number of bytes received for this SSRC.
            /// </summary>
            public ulong BytesReceived;

            /// <summary>
            /// Total number of frames correctly decoded for this RTP stream, that would be displayed
            /// if no frames are dropped.
            /// </summary>
            public uint FramesDecoded;

            #endregion
        }

        /// <summary>
        /// Subset of RTCTransportStats.
        /// See <see href="https://www.w3.org/TR/webrtc-stats/#transportstats-dict*"/>.
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct TransportStats
        {
            /// <summary>
            /// Unix timestamp (time since Epoch) of the statistics. For remote statistics, this is
            /// the time at which the information reached the local endpoint.
            /// </summary>
            public long TimestampUs;

            /// <summary>
            /// Total number of payload bytes sent on this <see cref="PeerConnection"/>, excluding
            /// headers and paddings.
            /// </summary>
            public ulong BytesSent;

            /// <summary>
            /// Total number of payload bytes received on this <see cref="PeerConnection"/>, excluding
            /// headers and paddings.
            /// </summary>
            public ulong BytesReceived;
        }

        /// <summary>
        /// Snapshot of the statistics relative to a peer connection/track.
        /// The various stats objects can be read through <see cref="GetStats{T}"/>.
        /// </summary>
        public class StatsReport : IDisposable
        {
            internal class Handle : SafeHandle
            {
                internal Handle(IntPtr h) : base(IntPtr.Zero, true) { handle = h; }
                public override bool IsInvalid => handle == IntPtr.Zero;
                protected override bool ReleaseHandle()
                {
                    PeerConnectionInterop.StatsReport_RemoveRef(handle);
                    return true;
                }
            }

            private Handle _handle;

            internal StatsReport(IntPtr h) { _handle = new Handle(h); }

            /// <summary>
            /// Get all the instances of a specific stats type in the report.
            /// </summary>
            /// <typeparam name="T">
            /// Must be one of <see cref="DataChannelStats"/>, <see cref="AudioSenderStats"/>,
            /// <see cref="AudioReceiverStats"/>, <see cref="VideoSenderStats"/>, <see cref="VideoReceiverStats"/>,
            /// <see cref="TransportStats"/>.
            /// </typeparam>
            public IEnumerable<T> GetStats<T>()
            {
                return PeerConnectionInterop.GetStatsObject<T>(_handle);
            }

            /// <summary>
            /// Dispose of the report.
            /// </summary>
            public void Dispose()
            {
                ((IDisposable)_handle).Dispose();
            }
        }

        /// <summary>
        /// Get a snapshot of the statistics relative to the peer connection.
        /// </summary>
        /// <exception xref="InvalidOperationException">The peer connection is not initialized.</exception>
        public Task<StatsReport> GetSimpleStatsAsync()
        {
            ThrowIfConnectionNotOpen();
            return PeerConnectionInterop.GetSimpleStatsAsync(_nativePeerhandle);
        }

        /// <summary>
        /// Utility to throw an exception if a method is called before the underlying
        /// native peer connection has been initialized.
        /// </summary>
        /// <exception xref="InvalidOperationException">The peer connection is not initialized.</exception>
        private void ThrowIfConnectionNotOpen()
        {
            lock (_openCloseLock)
            {
                if (_nativePeerhandle.IsClosed)
                {
                    MainEventSource.Log.PeerConnectionNotOpenError();
                    throw new InvalidOperationException("Cannot invoke native method with invalid peer connection handle.");
                }
            }
        }

        /// <summary>
        /// Get the list of video capture devices available on the local host machine.
        /// </summary>
        /// <returns>The list of available video capture devices.</returns>
        /// <remarks>
        /// Assign one of the returned <see cref="VideoCaptureDevice"/> to the <see cref="LocalVideoTrackSettings.videoDevice"/>
        /// field to force a local video track to use that device when creating it with
        /// <see cref="LocalVideoTrack.CreateFromDeviceAsync(LocalVideoTrackSettings)"/>.
        /// </remarks>
        public static Task<List<VideoCaptureDevice>> GetVideoCaptureDevicesAsync()
        {
            // Ensure the logging system is ready before using PInvoke.
            MainEventSource.Log.Initialize();

            // Always call this on a background thread, this is possibly the first call to the library so needs
            // to initialize the global factory, and that cannot be done from the main UI thread on UWP.
            return Task.Run(() =>
            {
                var devices = new List<VideoCaptureDevice>();
                var eventWaitHandle = new ManualResetEventSlim(initialState: false);
                var wrapper = new PeerConnectionInterop.EnumVideoCaptureDeviceWrapper()
                {
                    enumCallback = (id, name) =>
                    {
                        devices.Add(new VideoCaptureDevice() { id = id, name = name });
                    },
                    completedCallback = () =>
                    {
                        // On enumeration end, signal the caller thread
                        eventWaitHandle.Set();
                    },
                    // Keep delegates alive
                    EnumTrampoline = PeerConnectionInterop.VideoCaptureDevice_EnumCallback,
                    CompletedTrampoline = PeerConnectionInterop.VideoCaptureDevice_EnumCompletedCallback
                };

                // Prevent garbage collection of the wrapper delegates until the enumeration is completed.
                var handle = GCHandle.Alloc(wrapper, GCHandleType.Normal);
                IntPtr userData = GCHandle.ToIntPtr(handle);

                // Execute the native async callback
                uint res = PeerConnectionInterop.EnumVideoCaptureDevicesAsync(
                    wrapper.EnumTrampoline, userData, wrapper.CompletedTrampoline, userData);
                if (res != Utils.MRS_SUCCESS)
                {
                    // Clean-up and release the wrapper delegates
                    handle.Free();

                    Utils.ThrowOnErrorCode(res);
                    return null; // for the compiler
                }

                // Wait for end of enumerating
                eventWaitHandle.Wait();

                // Clean-up and release the wrapper delegates
                handle.Free();

                return devices;
            });
        }

        /// <summary>
        /// Enumerate the video capture formats for the specified video capture device.
        /// </summary>
        /// <param name="deviceId">Unique identifier of the video capture device to enumerate the
        /// capture formats of, as retrieved from the <see cref="VideoCaptureDevice.id"/> field of
        /// a capture device enumerated with <see cref="GetVideoCaptureDevicesAsync"/>.</param>
        /// <returns>The list of available video capture formats for the specified video capture device.</returns>
        public static Task<List<VideoCaptureFormat>> GetVideoCaptureFormatsAsync(string deviceId)
        {
            // Ensure the logging system is ready before using PInvoke.
            MainEventSource.Log.Initialize();

            // Always call this on a background thread, this is possibly the first call to the library so needs
            // to initialize the global factory, and that cannot be done from the main UI thread on UWP.
            return Task.Run(() =>
            {
                var formats = new List<VideoCaptureFormat>();
                var eventWaitHandle = new EventWaitHandle(initialState: false, EventResetMode.ManualReset);
                var wrapper = new PeerConnectionInterop.EnumVideoCaptureFormatsWrapper()
                {
                    enumCallback = (width, height, framerate, fourcc) =>
                    {
                        formats.Add(new VideoCaptureFormat() { width = width, height = height, framerate = framerate, fourcc = fourcc });
                    },
                    completedCallback = (Exception _) =>
                    {
                        // On enumeration end, signal the caller thread
                        eventWaitHandle.Set();
                    },
                    // Keep delegates alive
                    EnumTrampoline = PeerConnectionInterop.VideoCaptureFormat_EnumCallback,
                    CompletedTrampoline = PeerConnectionInterop.VideoCaptureFormat_EnumCompletedCallback
                };

                // Prevent garbage collection of the wrapper delegates until the enumeration is completed.
                var handle = GCHandle.Alloc(wrapper, GCHandleType.Normal);
                IntPtr userData = GCHandle.ToIntPtr(handle);

                // Execute the native async callback.
                uint res = PeerConnectionInterop.EnumVideoCaptureFormatsAsync(deviceId,
                    wrapper.EnumTrampoline, userData, wrapper.CompletedTrampoline, userData);
                if (res != Utils.MRS_SUCCESS)
                {
                    // Clean-up and release the wrapper delegates
                    handle.Free();

                    Utils.ThrowOnErrorCode(res);
                    return null; // for the compiler
                }

                // Wait for end of enumerating
                eventWaitHandle.WaitOne();

                // Clean-up and release the wrapper delegates
                handle.Free();

                return formats;
            });
        }

        /// <summary>
        /// Frame height round mode.
        /// </summary>
        /// <seealso cref="SetFrameHeightRoundMode(FrameHeightRoundMode)"/>
        public enum FrameHeightRoundMode
        {
            /// <summary>
            /// Leave frames unchanged.
            /// </summary>
            None = 0,

            /// <summary>
            /// Crop frame height to the nearest multiple of 16.
            /// ((height - nearestLowerMultipleOf16) / 2) rows are cropped from the top and
            /// (height - nearestLowerMultipleOf16 - croppedRowsTop) rows are cropped from the bottom.
            /// </summary>
            Crop = 1,

            /// <summary>
            /// Pad frame height to the nearest multiple of 16.
            /// ((nearestHigherMultipleOf16 - height) / 2) rows are added symmetrically at the top and
            /// (nearestHigherMultipleOf16 - height - addedRowsTop) rows are added symmetrically at the bottom.
            /// </summary>
            Pad = 2
        }

        /// <summary>
        /// [HoloLens 1 only]
        /// Use this function to select whether resolutions where height is not multiple of 16 pixels
        /// should be cropped, padded, or left unchanged.
        ///
        /// Default is <see cref="FrameHeightRoundMode.Crop"/> to avoid severe artifacts produced by
        /// the H.264 hardware encoder on HoloLens 1 due to a bug with the encoder. This is the
        /// recommended value, and should be used unless cropping discards valuable data in the top and
        /// bottom rows for a given usage, in which case <see cref="FrameHeightRoundMode.Pad"/> can
        /// be used as a replacement but may still produce some mild artifacts.
        ///
        /// This has no effect on other platforms.
        /// </summary>
        /// <param name="value">The rounding mode for video frames.</param>
        public static void SetFrameHeightRoundMode(FrameHeightRoundMode value)
        {
            Utils.SetFrameHeightRoundMode(value);
        }

        internal void OnConnected()
        {
            MainEventSource.Log.Connected();
            IsConnected = true;
            Connected?.Invoke();
        }

        internal void OnDataChannelAdded(DataChannel dataChannel)
        {
            MainEventSource.Log.DataChannelAdded(dataChannel.ID, dataChannel.Label);
            DataChannelAdded?.Invoke(dataChannel);
        }

        internal void OnDataChannelRemoved(DataChannel dataChannel)
        {
            MainEventSource.Log.DataChannelRemoved(dataChannel.ID, dataChannel.Label);
            DataChannelRemoved?.Invoke(dataChannel);
        }

        private static string ForceSdpCodecs(string sdp, string audio, string audioParams, string video, string videoParams)
        {
            if ((audio.Length > 0) || (video.Length > 0))
            {
                // +1 for space/semicolon before params.
                var initialLength = sdp.Length +
                    audioParams.Length + 1 +
                    videoParams.Length + 1;
                var builder = new StringBuilder(initialLength);
                ulong lengthInOut = (ulong)builder.Capacity + 1; // includes null terminator
                var audioFilter = new Utils.SdpFilter
                {
                    CodecName = audio,
                    ExtraParams = audioParams
                };
                var videoFilter = new Utils.SdpFilter
                {
                    CodecName = video,
                    ExtraParams = videoParams
                };

                uint res = Utils.SdpForceCodecs(sdp, audioFilter, videoFilter, builder, ref lengthInOut);
                if (res == Utils.MRS_E_INVALID_PARAMETER && lengthInOut > (ulong)builder.Capacity + 1)
                {
                    // New string is longer than the estimate (there might be multiple tracks).
                    // Increase the capacity and retry.
                    builder.Capacity = (int)lengthInOut - 1;
                    res = Utils.SdpForceCodecs(sdp, audioFilter, videoFilter, builder, ref lengthInOut);
                }
                Utils.ThrowOnErrorCode(res);
                builder.Length = (int)lengthInOut - 1; // discard the null terminator
                return builder.ToString();
            }
            return sdp;
        }

        /// <summary>
        /// Callback invoked by the internal WebRTC implementation when it needs a SDP message
        /// to be dispatched to the remote peer.
        /// </summary>
        /// <param name="type">The SDP message type.</param>
        /// <param name="sdp">The SDP message content.</param>
        internal void OnLocalSdpReadytoSend(string type, string sdp)
        {
            MainEventSource.Log.LocalSdpReady(type, sdp);

            // If the user specified a preferred audio or video codec, manipulate the SDP message
            // to exclude other codecs if the preferred one is supported.
            // Outgoing answers are filtered for the only purpose of adding the extra params to them.
            // The codec itself will have already been selected when filtering the incoming offer that prompted
            // the answer. Note that filtering the codec in the answer without doing it in
            // the offer first will leave the connection in an inconsistent state.
            string newSdp = ForceSdpCodecs(sdp: sdp,
                audio: PreferredAudioCodec,
                audioParams: PreferredAudioCodecExtraParamsRemote,
                video: PreferredVideoCodec,
                videoParams: PreferredVideoCodecExtraParamsRemote);

            LocalSdpReadytoSend?.Invoke(type, newSdp);
        }

        internal void OnIceCandidateReadytoSend(string candidate, int sdpMlineindex, string sdpMid)
        {
            MainEventSource.Log.IceCandidateReady(sdpMid, sdpMlineindex, candidate);
            IceCandidateReadytoSend?.Invoke(candidate, sdpMlineindex, sdpMid);
        }

        internal void OnIceStateChanged(IceConnectionState newState)
        {
            MainEventSource.Log.IceStateChanged(newState);
            IceStateChanged?.Invoke(newState);
        }

        internal void OnIceGatheringStateChanged(IceGatheringState newState)
        {
            MainEventSource.Log.IceGatheringStateChanged(newState);
            IceGatheringStateChanged?.Invoke(newState);
        }

        internal void OnRenegotiationNeeded()
        {
            MainEventSource.Log.RenegotiationNeeded();
            RenegotiationNeeded?.Invoke();
        }

        /// <summary>
        /// Insert the given transceiver into the <see cref="Transceivers"/> list at the index
        /// corresponding to its <see cref="Transceiver.MlineIndex"/>.
        /// </summary>
        /// <param name="transceiver">Transceiver object to insert.</param>
        internal void InsertTransceiverNoLock(Transceiver transceiver)
        {
            int mlineIndex = transceiver.MlineIndex;
            if (mlineIndex >= Transceivers.Count)
            {
                while (mlineIndex >= Transceivers.Count + 1)
                {
                    Transceivers.Add(null);
                }
                Transceivers.Add(transceiver);
            }
            else
            {
                Debug.Assert(Transceivers[mlineIndex] == null);
                Transceivers[mlineIndex] = transceiver;
            }
        }

        /// <summary>
        /// Callback on transceiver created for the peer connection, irrelevant of whether
        /// it has tracks or not. This is called both when created from the managed side or
        /// from the native side.
        /// </summary>
        /// <param name="tr">The newly created transceiver which has this peer connection as owner</param>
        internal void OnTransceiverAdded(AudioTransceiver tr)
        {
            lock (_tracksLock)
            {
                InsertTransceiverNoLock(tr);
            }
            AudioTransceiverAdded?.Invoke(tr);
        }

        /// <summary>
        /// Callback on transceiver created for the peer connection, irrelevant of whether
        /// it has tracks or not. This is called both when created from the managed side or
        /// from the native side.
        /// </summary>
        /// <param name="tr">The newly created transceiver which has this peer connection as owner</param>
        internal void OnTransceiverAdded(VideoTransceiver tr)
        {
            lock (_tracksLock)
            {
                InsertTransceiverNoLock(tr);
            }
            VideoTransceiverAdded?.Invoke(tr);
        }

        internal void OnAudioTrackAdded(RemoteAudioTrack track, AudioTransceiver transceiver)
        {
            MainEventSource.Log.AudioTrackAdded(track.Name);
            lock (_tracksLock)
            {
                if (!Transceivers.Contains(transceiver))
                {
                    InsertTransceiverNoLock(transceiver);
                }
            }
            track.OnTrackAddedToTransceiver(transceiver);
            AudioTrackAdded?.Invoke(track);
        }

        internal void OnAudioTrackRemoved(RemoteAudioTrack track)
        {
            MainEventSource.Log.AudioTrackRemoved(track.Name);
            AudioTransceiver transceiver = track.Transceiver; // cache before removed
            track.OnTrackRemoved(this);
            AudioTrackRemoved?.Invoke(transceiver, track);

            // PeerConnection is owning the remote track, and all internal states have been
            // updated and events fired, so dispose of the track now.
            track.Dispose();
        }

        /// <summary>
        /// Callback invoked when a transceiver starts receiving, and a remote video track is
        /// created as a result to receive its video data.
        /// </summary>
        /// <param name="track">The newly created remote video track.</param>
        /// <param name="transceiver">The video transceiver now receiving from the remote peer.</param>
        internal void OnVideoTrackAdded(RemoteVideoTrack track, VideoTransceiver transceiver)
        {
            MainEventSource.Log.VideoTrackAdded(track.Name);
            lock (_tracksLock)
            {
                if (!Transceivers.Contains(transceiver))
                {
                    InsertTransceiverNoLock(transceiver);
                }
            }
            track.OnTrackAddedToTransceiver(transceiver);
            VideoTrackAdded?.Invoke(track);
        }

        /// <summary>
        /// Callback invoked when a transceiver stops receiving, and a remote video track is
        /// removed from it as a result.
        /// </summary>
        /// <param name="track">The remote video track removed from the video transceiver.</param>
        internal void OnVideoTrackRemoved(RemoteVideoTrack track)
        {
            MainEventSource.Log.VideoTrackRemoved(track.Name);
            VideoTransceiver transceiver = track.Transceiver; // cache before removed
            track.OnTrackRemoved(this);
            VideoTrackRemoved?.Invoke(transceiver, track);

            // PeerConnection is owning the remote track, and all internal states have been
            // updated and events fired, so dispose of the track now.
            track.Dispose();
        }

        /// <summary>
        /// Callback invoked when a transceiver starts receiving, and a remote audio track is
        /// created as a result to receive its audio data.
        /// </summary>
        /// <param name="track">The newly created remote audio track.</param>
        internal void OnLocalTrackAdded(LocalAudioTrack track)
        {
            lock (_tracksLock)
            {
                Debug.Assert(!LocalAudioTracks.Contains(track));
                Debug.Assert(track.Transceiver != null);
                Debug.Assert(Transceivers.Contains(track.Transceiver));
                LocalAudioTracks.Add(track);
            }
        }

        /// <summary>
        /// Callback invoked when a transceiver stops receiving, and a remote audio track is
        /// removed from it as a result.
        /// </summary>
        /// <param name="track">The remote video track removed from the video transceiver.</param>
        internal void OnLocalTrackRemoved(LocalAudioTrack track)
        {
            lock (_tracksLock)
            {
                Debug.Assert(LocalAudioTracks.Contains(track));
                Debug.Assert(track.Transceiver != null);
                Debug.Assert(Transceivers.Contains(track.Transceiver));
                LocalAudioTracks.Remove(track);
            }
        }

        internal void OnLocalTrackAdded(LocalVideoTrack track)
        {
            lock (_tracksLock)
            {
                Debug.Assert(!LocalVideoTracks.Contains(track));
                Debug.Assert(track.Transceiver != null);
                Debug.Assert(Transceivers.Contains(track.Transceiver));
                LocalVideoTracks.Add(track);
            }
        }

        internal void OnLocalTrackRemoved(LocalVideoTrack track)
        {
            lock (_tracksLock)
            {
                Debug.Assert(LocalVideoTracks.Contains(track));
                Debug.Assert(track.Transceiver != null);
                Debug.Assert(Transceivers.Contains(track.Transceiver));
                LocalVideoTracks.Remove(track);
            }
        }

        internal void OnRemoteTrackAdded(RemoteAudioTrack track)
        {
            lock (_tracksLock)
            {
                Debug.Assert(!RemoteAudioTracks.Contains(track));
                Debug.Assert(track.Transceiver != null);
                Debug.Assert(Transceivers.Contains(track.Transceiver));
                RemoteAudioTracks.Add(track);
            }
        }

        internal void OnRemoteTrackRemoved(RemoteAudioTrack track)
        {
            lock (_tracksLock)
            {
                Debug.Assert(RemoteAudioTracks.Contains(track));
                Debug.Assert(track.Transceiver != null);
                Debug.Assert(Transceivers.Contains(track.Transceiver));
                RemoteAudioTracks.Remove(track);
            }
        }

        internal void OnRemoteTrackAdded(RemoteVideoTrack track)
        {
            lock (_tracksLock)
            {
                Debug.Assert(!RemoteVideoTracks.Contains(track));
                Debug.Assert(track.Transceiver != null);
                Debug.Assert(Transceivers.Contains(track.Transceiver));
                RemoteVideoTracks.Add(track);
            }
        }

        internal void OnRemoteTrackRemoved(RemoteVideoTrack track)
        {
            lock (_tracksLock)
            {
                Debug.Assert(RemoteVideoTracks.Contains(track));
                Debug.Assert(track.Transceiver != null);
                Debug.Assert(Transceivers.Contains(track.Transceiver));
                RemoteVideoTracks.Remove(track);
            }
        }

        /// <inheritdoc/>
        public override string ToString()
        {
            return $"(PeerConnection)\"{Name}\"";
        }
    }
}

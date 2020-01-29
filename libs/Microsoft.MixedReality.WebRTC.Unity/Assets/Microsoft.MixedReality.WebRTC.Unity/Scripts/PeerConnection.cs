// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.Events;
using System.Collections.Concurrent;
using System.Text;
using System.IO;

#if UNITY_WSA && !UNITY_EDITOR
using Windows.UI.Core;
using Windows.Foundation;
using Windows.Media.Core;
using Windows.Media.Capture;
using Windows.ApplicationModel.Core;
#endif

namespace Microsoft.MixedReality.WebRTC.Unity
{
    /// <summary>
    /// Enumeration of the different types of ICE servers.
    /// </summary>
    public enum IceType
    {
        /// <summary>
        /// Indicates there is no ICE information
        /// </summary>
        /// <remarks>
        /// Under normal use, this should not be used
        /// </remarks>
        None = 0,

        /// <summary>
        /// Indicates ICE information is of type STUN
        /// </summary>
        /// <remarks>
        /// https://en.wikipedia.org/wiki/STUN
        /// </remarks>
        Stun,

        /// <summary>
        /// Indicates ICE information is of type TURN
        /// </summary>
        /// <remarks>
        /// https://en.wikipedia.org/wiki/Traversal_Using_Relays_around_NAT
        /// </remarks>
        Turn
    }

    /// <summary>
    /// Represents an Ice server in a simple way that allows configuration from the unity inspector
    /// </summary>
    [Serializable]
    public struct ConfigurableIceServer
    {
        /// <summary>
        /// The type of the server
        /// </summary>
        [Tooltip("Type of ICE server")]
        public IceType Type;

        /// <summary>
        /// The unqualified uri of the server
        /// </summary>
        /// <remarks>
        /// You should not prefix this with "stun:" or "turn:"
        /// </remarks>
        [Tooltip("ICE server URI, without any stun: or turn: prefix.")]
        public string Uri;

        /// <summary>
        /// Convert the server to the representation the underlying libraries use
        /// </summary>
        /// <returns>stringified server information</returns>
        public override string ToString()
        {
            return string.Format("{0}:{1}", Type.ToString().ToLowerInvariant(), Uri);
        }
    }

    /// <summary>
    /// A <a href="https://docs.unity3d.com/ScriptReference/Events.UnityEvent.html">UnityEvent</a> that represents a WebRTC error event.
    /// </summary>
    [Serializable]
    public class WebRTCErrorEvent : UnityEvent<string>
    {
    }

    /// <summary>
    /// High-level wrapper for Unity WebRTC functionalities.
    /// This is the API entry point for establishing a connection with a remote peer.
    /// </summary>
    [AddComponentMenu("MixedReality-WebRTC/Peer Connection")]
    public class PeerConnection : MonoBehaviour
    {
        /// <summary>
        /// Retrieves the underlying peer connection object once initialized.
        /// </summary>
        /// <remarks>
        /// If <see cref="OnInitialized"/> has not fired, this will be <c>null</c>.
        /// </remarks>
        public WebRTC.PeerConnection Peer { get; private set; } = null;


        #region Behavior settings

        /// <summary>
        /// Flag to initialize the peer connection on <a href="https://docs.unity3d.com/ScriptReference/MonoBehaviour.Start.html">MonoBehaviour.Start()</a>.
        /// </summary>
        [Header("Behavior settings")]
        [Tooltip("Automatically initialize the peer connection on Start()")]
        public bool AutoInitializeOnStart = true;

        /// <summary>
        /// Flag to log all errors to the Unity console automatically.
        /// </summary>
        [Tooltip("Automatically log all errors to the Unity console")]
        public bool AutoLogErrorsToUnityConsole = true;

        #endregion


        #region Signaling

        /// <summary>
        /// Signaler to use to establish the connection.
        /// </summary>
        [Header("Signaling")]
        [Tooltip("Signaler to use to establish the connection")]
        public Signaler Signaler;

        #endregion


        #region Interactive Connectivity Establishment (ICE)

        /// <summary>
        /// Set of ICE servers the WebRTC library will use to try to establish a connection.
        /// </summary>
        [Header("ICE servers")]
        [Tooltip("Optional set of ICE servers (STUN and/or TURN)")]
        public List<ConfigurableIceServer> IceServers = new List<ConfigurableIceServer>()
        {
            new ConfigurableIceServer()
            {
                Type = IceType.Stun,
                Uri = "stun.l.google.com:19302"
            }
        };

        /// <summary>
        /// Optional username for the ICE servers.
        /// </summary>
        [Tooltip("Optional username for the ICE servers")]
        public string IceUsername;

        /// <summary>
        /// Optional credential for the ICE servers.
        /// </summary>
        [Tooltip("Optional credential for the ICE servers")]
        public string IceCredential;

        #endregion


        #region Events

        /// <summary>
        /// Event fired after the peer connection is initialized and ready for use.
        /// </summary>
        [Header("Peer connection events")]
        [Tooltip("Event fired after the peer connection is initialized and ready for use")]
        public UnityEvent OnInitialized = new UnityEvent();

        /// <summary>
        /// Event fired after the peer connection is shut down and cannot be used anymore.
        /// </summary>
        [Tooltip("Event fired after the peer connection is shut down and cannot be used anymore")]
        public UnityEvent OnShutdown = new UnityEvent();

        /// <summary>
        /// Event that occurs when a WebRTC error occurs
        /// </summary>
        [Tooltip("Event that occurs when a WebRTC error occurs")]
        public WebRTCErrorEvent OnError = new WebRTCErrorEvent();

        #endregion


        #region Private variables

        /// <summary>
        /// Internal queue used to marshal work back to the main Unity thread.
        /// </summary>
        private ConcurrentQueue<Action> _mainThreadWorkQueue = new ConcurrentQueue<Action>();

        /// <summary>
        /// Underlying native peer connection wrapper.
        /// </summary>
        /// <remarks>
        /// Unlike the public <see cref="Peer"/> property, this is never <c>NULL</c>,
        /// but can be an uninitialized peer.
        /// </remarks>
        private WebRTC.PeerConnection _nativePeer;

        private class AudioStreamInfo
        {
            public AudioSender Sender = null;
            public AudioReceiver Receiver = null;
            public AudioTransceiver Transceiver = null;
        }

        private class VideoStreamInfo
        {
            public VideoSender Sender = null;
            public VideoReceiver Receiver = null;
            public VideoTransceiver Transceiver = null;
        }

        /// <summary>
        /// Map of stream name to audio stream info.
        /// </summary>
        private Dictionary<string, AudioStreamInfo> _audioStreams = new Dictionary<string, AudioStreamInfo>();

        /// <summary>
        /// Map of stream name to video stream info.
        /// </summary>
        private Dictionary<string, VideoStreamInfo> _videoStreams = new Dictionary<string, VideoStreamInfo>();

        private bool _suspendOffers = false;

        #endregion


        #region Public methods

        /// <summary>
        /// Enumerate the video capture devices available as a WebRTC local video feed source.
        /// </summary>
        /// <returns>The list of local video capture devices available to WebRTC.</returns>
        public static Task<List<VideoCaptureDevice>> GetVideoCaptureDevicesAsync()
        {
            return WebRTC.PeerConnection.GetVideoCaptureDevicesAsync();
        }

        /// <summary>
        /// Initialize the underlying WebRTC libraries
        /// </summary>
        /// <remarks>
        /// This function is asynchronous, to monitor it's status bind a handler to OnInitialized and OnError
        /// </remarks>
        public Task InitializeAsync(CancellationToken token = default(CancellationToken))
        {
            // Normally would be initialized by Awake(), but in case the component is disabled
            if (_nativePeer == null)
            {
                _nativePeer = new WebRTC.PeerConnection();
                if (Signaler != null)
                {
                    _nativePeer.LocalSdpReadytoSend += Signaler_LocalSdpReadyToSend;
                    _nativePeer.IceCandidateReadytoSend += Signaler_IceCandidateReadytoSend;
                }
            }

            // if the peer is already set, we refuse to initialize again.
            // Note: for multi-peer scenarios, use multiple WebRTC components.
            if (_nativePeer.Initialized)
            {
                return Task.CompletedTask;
            }

#if UNITY_ANDROID
            AndroidJavaClass systemClass = new AndroidJavaClass("java.lang.System");
            string libname = "jingle_peerconnection_so";
            systemClass.CallStatic("loadLibrary", new object[1] { libname });
            Debug.Log("loadLibrary loaded : " + libname);

            /*
                * Below is equivalent of this java code:
                * PeerConnectionFactory.InitializationOptions.Builder builder = 
                *   PeerConnectionFactory.InitializationOptions.builder(UnityPlayer.currentActivity);
                * PeerConnectionFactory.InitializationOptions options = 
                *   builder.createInitializationOptions();
                * PeerConnectionFactory.initialize(options);
                */

            AndroidJavaClass playerClass = new AndroidJavaClass("com.unity3d.player.UnityPlayer");
            AndroidJavaObject activity = playerClass.GetStatic<AndroidJavaObject>("currentActivity");
            AndroidJavaClass webrtcClass = new AndroidJavaClass("org.webrtc.PeerConnectionFactory");
            AndroidJavaClass initOptionsClass = new AndroidJavaClass("org.webrtc.PeerConnectionFactory$InitializationOptions");
            AndroidJavaObject builder = initOptionsClass.CallStatic<AndroidJavaObject>("builder", new object[1] { activity });
            AndroidJavaObject options = builder.Call<AndroidJavaObject>("createInitializationOptions");

            if (webrtcClass != null)
            {
                webrtcClass.CallStatic("initialize", new object[1] { options });
            }
#endif

#if UNITY_WSA && !UNITY_EDITOR
            if (UnityEngine.WSA.Application.RunningOnUIThread())
#endif
            {
                return RequestAccessAndInitAsync(token);
            }
#if UNITY_WSA && !UNITY_EDITOR
            else
            {
                UnityEngine.WSA.Application.InvokeOnUIThread(() => RequestAccessAndInitAsync(token), waitUntilDone: true);
                return Task.CompletedTask;
            }
#endif
        }

        /// <summary>
        /// Register an audio sender source with the peer connection, for automated track pairing.
        /// </summary>
        /// <param name="source">The audio sender to register</param>
        /// <remarks>
        /// Only a single audio sender can be registered for a given stream name (<see cref="AudioSender.TrackName"/>).
        /// Attempting to register more than one will yield a <see xref="System.InvalidOperationException"/>, and generally
        /// indicates that multiple <see cref="AudioSender"/> components have the same values for the
        /// <see cref="AudioSender.TrackName"/> and <see cref="AudioSender.PeerConnection"/> properties.
        /// 
        /// This change internally generates a renegotiation needed event on the current Unity peer connection component,
        /// which creates a new offer unless auto-offers are suspended or the peer is not yet connected.
        /// </remarks>
        public void RegisterSource(AudioSender source)
        {
            if (_audioStreams.TryGetValue(source.TrackName, out AudioStreamInfo streamInfo))
            {
                if (streamInfo.Sender != null)
                {
                    throw new InvalidOperationException($"Peer connection '{name}' has already an audio sender '{streamInfo.Sender.name}' for source '{source.TrackName}', cannot add also '{source.name}'.");
                }
                streamInfo.Sender = source;
            }
            else
            {
                _audioStreams.Add(source.TrackName, new AudioStreamInfo { Sender = source });
            }
            Peer_RenegotiationNeeded();
        }

        /// <summary>
        /// Register a video sender source with the peer connection, for automated track pairing.
        /// </summary>
        /// <param name="source">The video sender to register</param>
        /// <remarks>
        /// Only a single video sender can be registered for a given stream name (<see cref="VideoSender.TrackName"/>).
        /// Attempting to register more than one will yield a <see xref="System.InvalidOperationException"/>, and generally
        /// indicates that multiple <see cref="VideoSender"/> components have the same values for the
        /// <see cref="VideoSender.TrackName"/> and <see cref="VideoSender.PeerConnection"/> properties.
        /// 
        /// This change internally generates a renegotiation needed event on the current Unity peer connection component,
        /// which creates a new offer unless auto-offers are suspended or the peer is not yet connected.
        /// </remarks>
        public void RegisterSource(VideoSender source)
        {
            if (_videoStreams.TryGetValue(source.TrackName, out VideoStreamInfo streamInfo))
            {
                if (streamInfo.Sender != null)
                {
                    throw new InvalidOperationException($"Peer connection '{name}' has already a video sender '{streamInfo.Sender.name}' for source '{source.TrackName}', cannot add also '{source.name}'.");
                }
                streamInfo.Sender = source;
            }
            else
            {
                _videoStreams.Add(source.TrackName, new VideoStreamInfo { Sender = source });
            }
            Peer_RenegotiationNeeded();
        }

        /// <summary>
        /// Register an audio receiver source with the peer connection, for automated track pairing.
        /// </summary>
        /// <param name="source">The audio receiver to register</param>
        /// <remarks>
        /// Only a single audio receiver can be registered for a given stream name (<see cref="AudioReceiver.TargetTransceiverName"/>).
        /// Attempting to register more than one will yield a <see xref="System.InvalidOperationException"/>, and generally
        /// indicates that multiple <see cref="AudioReceiver"/> components have the same values for the
        /// <see cref="AudioReceiver.TargetTransceiverName"/> and <see cref="AudioReceiver.PeerConnection"/> properties.
        /// 
        /// This change internally generates a renegotiation needed event on the current Unity peer connection component,
        /// which creates a new offer unless auto-offers are suspended or the peer is not yet connected.
        /// </remarks>
        public void RegisterSource(AudioReceiver source)
        {
            if (_audioStreams.TryGetValue(source.TargetTransceiverName, out AudioStreamInfo streamInfo))
            {
                if (streamInfo.Receiver != null)
                {
                    throw new InvalidOperationException($"Peer connection '{name}' has already an audio receiver '{streamInfo.Receiver.name}' for source '{source.TargetTransceiverName}', cannot add also '{source.name}'.");
                }
                streamInfo.Receiver = source;
            }
            else
            {
                _audioStreams.Add(source.TargetTransceiverName, new AudioStreamInfo { Receiver = source });
            }
            Peer_RenegotiationNeeded();
        }

        /// <summary>
        /// Register a video receiver source with the peer connection, for automated track pairing.
        /// </summary>
        /// <param name="source">The video receiver to register</param>
        /// <remarks>
        /// Only a single video receiver can be registered for a given stream name (<see cref="AudioSender.TargetTransceiverName"/>).
        /// Attempting to register more than one will yield a <see xref="System.InvalidOperationException"/>, and generally
        /// indicates that multiple <see cref="VideoReceiver"/> components have the same values for the
        /// <see cref="VideoReceiver.TargetTransceiverName"/> and <see cref="VideoReceiver.PeerConnection"/> properties.
        /// 
        /// This change internally generates a renegotiation needed event on the current Unity peer connection component,
        /// which creates a new offer unless auto-offers are suspended or the peer is not yet connected.
        /// </remarks>
        public void RegisterSource(VideoReceiver source)
        {
            if (_videoStreams.TryGetValue(source.TargetTransceiverName, out VideoStreamInfo streamInfo))
            {
                if (streamInfo.Receiver != null)
                {
                    throw new InvalidOperationException($"Peer connection '{name}' has already a video receiver '{streamInfo.Receiver.name}' for source '{source.TargetTransceiverName}', cannot add also '{source.name}'.");
                }
                streamInfo.Receiver = source;
            }
            else
            {
                _videoStreams.Add(source.TargetTransceiverName, new VideoStreamInfo { Receiver = source });
            }
            Peer_RenegotiationNeeded();
        }

        /// <summary>
        /// Create a new connection offer, either for a first connection to the remote peer, or for
        /// renegotiating some new or removed tracks.
        /// </summary>
        public bool CreateOffer()
        {
            if (Peer == null)
            {
                throw new InvalidOperationException("Cannot create an offer with an uninitialized peer.");
            }

            // Batch all changes into a single offer
            _suspendOffers = true;

            // Add all new transceivers for local tracks
            foreach (var pair in _audioStreams)
            {
                var streamId = pair.Key;
                var streamInfo = pair.Value;

                // Compute the transceiver desired direction based on what the local peer expects, both in terms
                // of sending and in terms of receiving. Note that this means the remote peer will not be able to
                // send any data if the local peer did not add a remote source first.
                // Tracks are not tested explicitly since the local track can be swapped on-the-fly without renegotiation,
                // and the remote track is generally not added yet at the beginning of the negotiation.
                bool wantsSend = (streamInfo.Sender != null);
                bool wantsRecv = (streamInfo.Receiver != null);
                var wantsDir = (wantsSend ? (wantsRecv ? Transceiver.Direction.SendReceive : Transceiver.Direction.SendOnly)
                    : (wantsRecv ? Transceiver.Direction.ReceiveOnly : Transceiver.Direction.Inactive));

                // Create a new transceiver if none exists
                if (streamInfo.Transceiver == null)
                {
                    var settings = new AudioTransceiverInitSettings
                    {
                        Name = streamId,
                        InitialDesiredDirection = wantsDir
                    };
                    streamInfo.Transceiver = _nativePeer.AddAudioTransceiver(settings);
                }
                else
                {
                    streamInfo.Transceiver.DesiredDirection = wantsDir;
                }

                // Associate the tracks with the transceiver, if any
                if (wantsSend)
                {
                    streamInfo.Transceiver.LocalTrack = streamInfo.Sender.Track;
                }
                //if (hasRecv)
                //{
                //    streamInfo.Transceiver.RemoteTrack = streamInfo.Receiver.Track;
                //}
            }
            foreach (var pair in _videoStreams)
            {
                var streamId = pair.Key;
                var streamInfo = pair.Value;

                // Compute the transceiver desired direction based on what the local peer expects, both in terms
                // of sending and in terms of receiving. Note that this means the remote peer will not be able to
                // send any data if the local peer did not add a remote source first.
                // Tracks are not tested explicitly since the local track can be swapped on-the-fly without renegotiation,
                // and the remote track is generally not added yet at the beginning of the negotiation.
                bool wantsSend = (streamInfo.Sender != null);
                bool wantsRecv = (streamInfo.Receiver != null);
                var wantsDir = (wantsSend ? (wantsRecv ? Transceiver.Direction.SendReceive : Transceiver.Direction.SendOnly)
                    : (wantsRecv ? Transceiver.Direction.ReceiveOnly : Transceiver.Direction.Inactive));

                // Create a new transceiver if none exists
                if (streamInfo.Transceiver == null)
                {
                    var settings = new VideoTransceiverInitSettings
                    {
                        Name = streamId,
                        InitialDesiredDirection = wantsDir
                    };
                    streamInfo.Transceiver = _nativePeer.AddVideoTransceiver(settings);
                }
                else
                {
                    streamInfo.Transceiver.DesiredDirection = wantsDir;
                }

                // Associate the tracks with the transceiver, if any
                if (wantsSend)
                {
                    streamInfo.Transceiver.LocalTrack = streamInfo.Sender.Track;
                }
                //if (hasRecv)
                //{
                //    streamInfo.Transceiver.RemoteTrack = streamInfo.Receiver.Track;
                //}
            }

            // Create the offer
            _suspendOffers = false;
            return _nativePeer.CreateOffer();
        }

        /// <summary>
        /// Create an answer to a previously received offer.
        /// </summary>
        public bool CreateAnswer()
        {
            if (Peer == null)
            {
                throw new InvalidOperationException("Cannot create an offer with an uninitialized peer.");
            }

            // Remote description has been applied to the peer connection, creating any transceiver that the
            // remote peer wants added, either to send or to receive (or both). Now match that list with the
            // list of local streams that this peer wants added, and add the ones missing, while updating the
            // stream info for existing ones.
            {
                var transceivers = _nativePeer.AudioTransceivers;
                foreach (var pair in _audioStreams)
                {
                    var streamId = pair.Key;
                    var streamInfo = pair.Value;

                    // Note: do not use the ?. operator with Unity objects
                    bool wantsSend = (streamInfo.Sender != null);
                    bool wantsRecv = (streamInfo.Receiver != null);

                    // Try to find a matching transceiver, or create a new one
                    var tr = transceivers.Find(t => t.Name == streamId);
                    if (tr != null)
                    {
                        streamInfo.Transceiver = tr;

                        // If sending, fix up transceiver direction. Because the remote description was already applied before
                        // any sending transceiver was added, all transceivers automatically added are in ReceiveOnly or Inactive
                        // state, since the implementation couldn't associate them with existing senders (none were added yet).
                        // This ensures that a TrackAdded event is fired on the remote peer when it receives the answer.
                        if (wantsSend)
                        {
                            var desDir = tr.DesiredDirection;
                            if (desDir == Transceiver.Direction.ReceiveOnly)
                            {
                                tr.DesiredDirection = Transceiver.Direction.SendReceive;
                            }
                            else if (desDir == Transceiver.Direction.Inactive)
                            {
                                tr.DesiredDirection = Transceiver.Direction.SendOnly;
                            }
                        }
                    }
                    else
                    {
                        // Create a new transceiver for the video stream
                        var wantsDir = (wantsSend ? (wantsRecv ? Transceiver.Direction.SendReceive : Transceiver.Direction.ReceiveOnly)
                            : (wantsRecv ? Transceiver.Direction.ReceiveOnly : Transceiver.Direction.Inactive));
                        var settings = new AudioTransceiverInitSettings
                        {
                            Name = streamId,
                            InitialDesiredDirection = wantsDir
                        };
                        tr = _nativePeer.AddAudioTransceiver(settings);
                        streamInfo.Transceiver = tr;
                    }

                    // Associate the tracks with the transceiver, if any
                    if (wantsSend)
                    {
                        tr.LocalTrack = streamInfo.Sender.Track;
                    }
                    //if (wantsRecv)
                    //{
                    //    Debug.Assert(tr.RemoteTrack != null);
                    //    Debug.Assert(tr.RemoteTrack == streamInfo.Receiver.Track);
                    //}
                }
            }
            {
                var transceivers = _nativePeer.VideoTransceivers;
                foreach (var pair in _videoStreams)
                {
                    var streamId = pair.Key;
                    var streamInfo = pair.Value;

                    // Note: do not use the ?. operator with Unity objects
                    bool wantsSend = (streamInfo.Sender != null);
                    bool wantsRecv = (streamInfo.Receiver != null);

                    // Try to find a matching transceiver, or create a new one
                    var tr = transceivers.Find(t => t.Name == streamId);
                    if (tr != null)
                    {
                        streamInfo.Transceiver = tr;

                        // If sending, fix up transceiver direction. Because the remote description was already applied before
                        // any sending transceiver was added, all transceivers automatically added are in ReceiveOnly or Inactive
                        // state, since the implementation couldn't associate them with existing senders (none were added yet).
                        // This ensures that a TrackAdded event is fired on the remote peer when it receives the answer.
                        if (wantsSend)
                        {
                            var desDir = tr.DesiredDirection;
                            if (desDir == Transceiver.Direction.ReceiveOnly)
                            {
                                tr.DesiredDirection = Transceiver.Direction.SendReceive;
                            }
                            else if (desDir == Transceiver.Direction.Inactive)
                            {
                                tr.DesiredDirection = Transceiver.Direction.SendOnly;
                            }
                        }
                    }
                    else
                    {
                        // Create a new transceiver for the video stream
                        var wantsDir = (wantsSend ? (wantsRecv ? Transceiver.Direction.SendReceive : Transceiver.Direction.SendOnly)
                            : (wantsRecv ? Transceiver.Direction.ReceiveOnly : Transceiver.Direction.Inactive));
                        var settings = new VideoTransceiverInitSettings
                        {
                            Name = streamId,
                            InitialDesiredDirection = wantsDir
                        };
                        tr = _nativePeer.AddVideoTransceiver(settings);
                        streamInfo.Transceiver = tr;
                    }

                    // Associate the tracks with the transceiver, if any
                    if (wantsSend)
                    {
                        tr.LocalTrack = streamInfo.Sender.Track;
                    }
                    //if (wantsRecv)
                    //{
                    //    Debug.Assert(tr.RemoteTrack != null);
                    //    Debug.Assert(tr.RemoteTrack == streamInfo.Receiver.Track);
                    //}
                }
            }

            // Create the answer
            return _nativePeer.CreateAnswer();
        }

        /// <summary>
        /// Uninitialize the underlying WebRTC library, effectively cleaning up the allocated peer connection.
        /// </summary>
        /// <remarks>
        /// <see cref="Peer"/> will be <c>null</c> afterward.
        /// </remarks>
        public void Uninitialize()
        {
            if ((_nativePeer != null) && _nativePeer.Initialized)
            {
                // Fire signals before doing anything else to allow listeners to clean-up,
                // including un-registering any callback and remove any track from the connection.
                OnShutdown.Invoke();

                if (Signaler != null)
                {
                    Signaler.OnMessage -= Signaler_OnMessage;
                    Signaler.OnPeerUninitializing(this);
                }

                // Prevent publicly accessing the native peer after it has been deinitialized.
                // This does not prevent systems caching a reference from accessing it, but it
                // is their responsibility to check that the peer is initialized.
                Peer = null;

                // Close the connection and release native resources.
                _nativePeer.Dispose();
            }
        }

        #endregion


        #region Unity MonoBehaviour methods

        private void Awake()
        {
            _nativePeer = new WebRTC.PeerConnection();
            if (Signaler != null)
            {
                _nativePeer.LocalSdpReadytoSend += Signaler_LocalSdpReadyToSend;
                _nativePeer.IceCandidateReadytoSend += Signaler_IceCandidateReadytoSend;
            }
        }

        /// <summary>
        /// Unity Engine Start() hook
        /// </summary>
        /// <remarks>
        /// See <see href="https://docs.unity3d.com/ScriptReference/MonoBehaviour.Start.html"/>
        /// </remarks>
        private void Start()
        {
            if (AutoLogErrorsToUnityConsole)
            {
                OnError.AddListener(OnError_Listener);
            }

            //// List video capture devices to Unity console
            //GetVideoCaptureDevicesAsync().ContinueWith((prevTask) =>
            //{
            //    var devices = prevTask.Result;
            //    _mainThreadWorkQueue.Enqueue(() =>
            //    {
            //        foreach (var device in devices)
            //        {
            //            Debug.Log($"Found video capture device '{device.name}' (id:{device.id}).");
            //        }
            //    });
            //});

            if (AutoInitializeOnStart)
            {
                InitializeAsync();
            }
        }

        /// <summary>
        /// Unity Engine Update() hook
        /// </summary>
        /// <remarks>
        /// https://docs.unity3d.com/ScriptReference/MonoBehaviour.Update.html
        /// </remarks>
        private void Update()
        {
            // Execute any pending work enqueued by background tasks
            while (_mainThreadWorkQueue.TryDequeue(out Action workload))
            {
                workload();
            }
        }

        /// <summary>
        /// Unity Engine OnDestroy() hook
        /// </summary>
        /// <remarks>
        /// https://docs.unity3d.com/ScriptReference/MonoBehaviour.OnDestroy.html
        /// </remarks>
        private void OnDestroy()
        {
            if (Signaler != null)
            {
                _nativePeer.IceCandidateReadytoSend -= Signaler_IceCandidateReadytoSend;
                _nativePeer.LocalSdpReadytoSend -= Signaler_LocalSdpReadyToSend;
            }
            Uninitialize();
            OnError.RemoveListener(OnError_Listener);
        }

#endregion


        #region Private implementation

        /// <summary>
        /// Internal helper to ensure device access and continue initialization.
        /// </summary>
        /// <remarks>
        /// On UWP this must be called from the main UI thread.
        /// </remarks>
        private Task RequestAccessAndInitAsync(CancellationToken token)
        {
#if UNITY_WSA && !UNITY_EDITOR
            // On UWP the app must have the "webcam" capability, and the user must allow webcam
            // access. So check that access before trying to initialize the WebRTC library, as this
            // may result in a popup window being displayed the first time, which needs to be accepted
            // before the camera can be accessed by WebRTC.
            var mediaAccessRequester = new MediaCapture();
            var mediaSettings = new MediaCaptureInitializationSettings();
            mediaSettings.AudioDeviceId = "";
            mediaSettings.VideoDeviceId = "";
            mediaSettings.StreamingCaptureMode = StreamingCaptureMode.AudioAndVideo;
            mediaSettings.PhotoCaptureSource = PhotoCaptureSource.VideoPreview;
            mediaSettings.SharingMode = MediaCaptureSharingMode.SharedReadOnly; // for MRC and lower res camera
            var accessTask = mediaAccessRequester.InitializeAsync(mediaSettings).AsTask(token);
            return accessTask.ContinueWith(prevTask =>
            {
                token.ThrowIfCancellationRequested();

                if (prevTask.Exception == null)
                {
                    InitializePluginAsync(token);
                }
                else
                {
                    _mainThreadWorkQueue.Enqueue(() =>
                    {
                        OnError.Invoke($"Audio/Video access failure: {prevTask.Exception.Message}.");
                    });
                }
            }, token);
#else
            return InitializePluginAsync(token);
#endif
        }

        /// <summary>
        /// Internal handler to actually initialize the 
        /// </summary>
        private Task InitializePluginAsync(CancellationToken token)
        {
            Debug.Log("Initializing WebRTC plugin...");
            var config = new PeerConnectionConfiguration();
            foreach (var server in IceServers)
            {
                config.IceServers.Add(new IceServer
                {
                    Urls = { server.ToString() },
                    TurnUserName = IceUsername,
                    TurnPassword = IceCredential
                });
            }
            return _nativePeer.InitializeAsync(config, token).ContinueWith((initTask) =>
            {
                token.ThrowIfCancellationRequested();

                if (initTask.Exception != null)
                {
                    _mainThreadWorkQueue.Enqueue(() =>
                    {
                        var errorMessage = new StringBuilder();
                        errorMessage.Append("WebRTC plugin initializing failed. See full log for exception details.\n");
                        Exception ex = initTask.Exception;
                        while (ex is AggregateException ae)
                        {
                            errorMessage.Append($"AggregationException: {ae.Message}\n");
                            ex = ae.InnerException;
                        }
                        errorMessage.Append($"Exception: {ex.Message}");
                        OnError.Invoke(errorMessage.ToString());
                    });
                    throw initTask.Exception;
                }

                _mainThreadWorkQueue.Enqueue(OnPostInitialize);
            }, token);
        }

        /// <summary>
        /// Callback fired on the main UI thread once the WebRTC plugin was initialized successfully.
        /// </summary>
        private void OnPostInitialize()
        {
            Debug.Log("WebRTC plugin initialized successfully.");

            _nativePeer.RenegotiationNeeded += Peer_RenegotiationNeeded;

            // Once the peer is initialized, it becomes publicly accessible.
            // This prevent scripts from accessing it before it is initialized,
            // or worse before it is constructed in Awake(). This happens because
            // some scripts try to access Peer in OnEnabled(), which won't work
            // if Unity decided to initialize that script before the current one.
            // However subsequent calls will (and should) work as expected.
            Peer = _nativePeer;

            if (Signaler != null)
            {
                Signaler.OnPeerInitialized(this);
                Signaler.OnMessage += Signaler_OnMessage;
            }
            OnInitialized.Invoke();
        }

        private void Signaler_LocalSdpReadyToSend(string type, string sdp)
        {
            var message = new Signaler.Message
            {
                MessageType = Signaler.Message.WireMessageTypeFromString(type),
                Data = sdp
            };
            Signaler.SendMessageAsync(message);
        }

        private void Signaler_IceCandidateReadytoSend(string candidate, int sdpMlineindex, string sdpMid)
        {
            var message = new Signaler.Message
            {
                MessageType = Signaler.Message.WireMessageType.Ice,
                Data = $"{candidate}|{sdpMlineindex}|{sdpMid}",
                IceDataSeparator = "|"
            };
            Signaler.SendMessageAsync(message);
        }

        private async void Signaler_OnMessage(Signaler.Message message)
        {
            switch (message.MessageType)
            {
                case Signaler.Message.WireMessageType.Offer:
                    await _nativePeer.SetRemoteDescriptionAsync("offer", message.Data);
                    // If we get an offer, we immediately send an answer back
                    _nativePeer.CreateAnswer();
                    break;

                case Signaler.Message.WireMessageType.Answer:
                    await _nativePeer.SetRemoteDescriptionAsync("answer", message.Data);
                    break;

                case Signaler.Message.WireMessageType.Ice:
                    // TODO - This is NodeDSS-specific
                    // this "parts" protocol is defined above, in OnIceCandiateReadyToSend listener
                    var parts = message.Data.Split(new string[] { message.IceDataSeparator }, StringSplitOptions.RemoveEmptyEntries);
                    // Note the inverted arguments; candidate is last here, but first in OnIceCandiateReadyToSend
                    _nativePeer.AddIceCandidate(parts[2], int.Parse(parts[1]), parts[0]);
                    break;

                default:
                    throw new InvalidOperationException($"Unhandled signaler message type '{message.MessageType}'");
            }
        }

        private void Peer_RenegotiationNeeded()
        {
            // If already connected, update the connection on the fly.
            // If not, wait for user action and don't automatically connect.
            if (_nativePeer.IsConnected && !_suspendOffers)
            {
                CreateOffer();
            }
        }

        /// <summary>
        /// Internal handler for on-error, if <see cref="AutoLogErrorsToUnityConsole"/> is <c>true</c>
        /// </summary>
        /// <param name="error">The error message</param>
        private void OnError_Listener(string error)
        {
            Debug.LogError(error);
        }

        #endregion
    }
}

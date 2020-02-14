// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.Events;
using System.Collections.Concurrent;
using System.Text;
using System.ComponentModel;

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

    [Serializable]
    public class TransceiverInfo
    {
        public MediaKind Kind;
        public MediaSender Sender;
        public MediaReceiver Receiver;

        [NonSerialized]
        public Transceiver Transceiver;
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
        [Header("Peer Connection Events")]
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

        /// <summary>
        /// List of transceivers and their associated media sender/receiver components.
        /// </summary>
        [SerializeField]
        private List<TransceiverInfo> _transceivers = new List<TransceiverInfo>();

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
            // Check in case Awake() was called first
            if (_nativePeer == null)
            {
                CreateNativePeerConnection();
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

        public TransceiverInfo AddTransceiver(MediaKind kind)
        {
            var tr = new TransceiverInfo { Kind = kind };
            _transceivers.Add(tr);
            return tr;
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

            // Add all new transceivers for local tracks. Since transceivers are only paired by negotiated mid,
            // we need to know which peer sends the offer before adding the transceivers on the offering side only,
            // and then pair them on the receiving side. Otherwise they are duplicated, as the transceiver mid from
            // locally-created transceivers is not negotiated yet, so ApplyRemoteDescriptionAsync() won't be able
            // to find them and will re-create a new set of transceivers, leading to duplicates.
            // So we wait until we know this peer is the offering side, and add transceivers to it right before
            // creating an offer. The remote peer will then match the transceivers by mlineIndex, then add any missing
            // ones, after it applied the offer.
            {
                // In case this is not the first offer, some transceivers might already exist
                var transceivers = _nativePeer.Transceivers;
                int numNativeTransceivers = transceivers.Count;

                for (int mlineIndex = 0; mlineIndex < _transceivers.Count; ++mlineIndex)
                {
                    var transceiverInfo = _transceivers[mlineIndex];

                    // Compute the transceiver desired direction based on what the local peer expects, both in terms
                    // of sending and in terms of receiving. Note that this means the remote peer will not be able to
                    // send any data if the local peer did not add a remote source first.
                    // Tracks are not tested explicitly since the local track can be swapped on-the-fly without renegotiation,
                    // and the remote track is generally not added yet at the beginning of the negotiation, but only when
                    // the remote description is applied (so for the offering side, at the end of the exchange when the
                    // answer is received).
                    bool wantsSend = (transceiverInfo.Sender != null);
                    bool wantsRecv = (transceiverInfo.Receiver != null);
                    var wantsDir = (wantsSend ? (wantsRecv ? Transceiver.Direction.SendReceive : Transceiver.Direction.SendOnly)
                        : (wantsRecv ? Transceiver.Direction.ReceiveOnly : Transceiver.Direction.Inactive));

                    // Create a new transceiver if none exists
                    if (transceiverInfo.Transceiver == null)
                    {
                        if (mlineIndex < transceivers.Count)
                        {
                            // If kind mismatch, likely the transceiver order was changed, which is not allows.
                            // Transceivers are immutably mapped to a unique mid when created.
                            //< FIXME - What about stopped transceivers, is mlineIndex reused?
                            Debug.Assert(transceiverInfo.Kind == transceivers[mlineIndex].MediaKind);

                            // Use an existing transceiver created during a previous negotiation
                            transceiverInfo.Transceiver = transceivers[mlineIndex];
                            transceiverInfo.Transceiver.DesiredDirection = wantsDir;
                        }
                        else
                        {
                            // Create a new transceiver if none exists
                            if (transceiverInfo.Kind == MediaKind.Audio)
                            {
                                var settings = new AudioTransceiverInitSettings
                                {
                                    Name = $"mrsw#{mlineIndex}",
                                    InitialDesiredDirection = wantsDir
                                };
                                transceiverInfo.Transceiver = _nativePeer.AddAudioTransceiver(settings);
                            }
                            else if (transceiverInfo.Kind == MediaKind.Video)
                            {
                                var settings = new VideoTransceiverInitSettings
                                {
                                    Name = $"mrsw#{mlineIndex}",
                                    InitialDesiredDirection = wantsDir
                                };
                                transceiverInfo.Transceiver = _nativePeer.AddVideoTransceiver(settings);
                            }
                            else
                            {
                                throw new InvalidEnumArgumentException("Unknown media kind for transceiver");
                            }
                        }
                    }
                    else
                    {
                        // Update the existing transceiver direction
                        transceiverInfo.Transceiver.DesiredDirection = wantsDir;
                    }

                    // Create the local sender track and attach it to the transceiver
                    if (wantsSend)
                    {
                        if (transceiverInfo.Kind == MediaKind.Audio)
                        {
                            var audioTransceiver = (AudioTransceiver)transceiverInfo.Transceiver;
                            var audioSender = (AudioSender)transceiverInfo.Sender;
                            //< FIXME - CreateOfferAsync()
                            //await audioSender.InitAndAttachTrackAsync(audioTransceiver);
                            audioSender.InitAndAttachTrackAsync(audioTransceiver).Wait();
                        }
                        else if (transceiverInfo.Kind == MediaKind.Video)
                        {
                            var videoTransceiver = (VideoTransceiver)transceiverInfo.Transceiver;
                            var videoSender = (VideoSender)transceiverInfo.Sender;
                            //< FIXME - CreateOfferAsync()
                            //await videoSender.InitAndAttachTrackAsync(videoTransceiver);
                            videoSender.InitAndAttachTrackAsync(videoTransceiver).Wait();
                        }
                    }

                    // The remote track is only created when applying a description, so only attach
                    // the transceiver for now.
                    if (wantsRecv)
                    {
                        if (transceiverInfo.Kind == MediaKind.Audio)
                        {
                            var audioTransceiver = (AudioTransceiver)transceiverInfo.Transceiver;
                            var audioReceiver = (AudioReceiver)transceiverInfo.Receiver;
                            audioReceiver.AttachToTransceiver(audioTransceiver);
                        }
                        else if (transceiverInfo.Kind == MediaKind.Video)
                        {
                            var videoTransceiver = (VideoTransceiver)transceiverInfo.Transceiver;
                            var videoReceiver = (VideoReceiver)transceiverInfo.Receiver;
                            videoReceiver.AttachToTransceiver(videoTransceiver);
                        }
                    }
                }

                // Ignore extra transceivers without a registered component to attach
                for (int i = _transceivers.Count; i < numNativeTransceivers; ++i)
                {
                    _mainThreadWorkQueue.Enqueue(() =>
                    {
                        Debug.LogWarning($"Peer connection {name} has transceiver #{i} but no sender/receiver component to process it. The transceiver will be ignored.");
                    });
                }
            }

            // Create the offer
            _suspendOffers = false;
            return _nativePeer.CreateOffer();
        }

        /// <summary>
        /// Pass the given SDP description received from the remote peer via signaling to the
        /// underlying WebRTC implementation, which will parse and use it.
        ///
        /// This must be called by the signaler when receiving a message. Once this operation
        /// has completed, it is safe to call <see cref="CreateAnswer"/>.
        /// 
        /// This implementation does some extra work compared to the lower level implementation
        /// <see cref="WebRTC.PeerConnection.SetRemoteDescriptionAsync(string, string)"/> found
        /// in the C# library, and must be called instead of it when using the Unity integration.
        /// This ensures media receiver components are correctly paired with the remote tracks.
        /// </summary>
        /// <param name="type">The type of SDP message ("offer" or "answer")</param>
        /// <param name="sdp">The content of the SDP message</param>
        /// <returns>Returns a task which completes once the remote description has been applied and transceivers
        /// have been updated.</returns>
        /// <exception xref="InvalidOperationException">The peer connection is not intialized.</exception>
        public Task SetRemoteDescriptionAsync(string type, string sdp)
        {
            // First apply the remote description
            var task = Peer.SetRemoteDescriptionAsync(type, sdp);

            // Once applied, try to pair transceivers and remote tracks with the Unity receiver components
            return task.ContinueWith(async _ =>
            {
                // If receiving an offer, this is the first opportunity for the answering side to add its local tracks
                // (media senders) and any extra transceiver it wants. The offering peer already did that in CreateOffer().
                if (type == "offer")
                {
                    // Remote description has been applied to the peer connection receiving the offer (above). That
                    // created any transceiver that the remote peer wants added, either to send or to receive (or both).
                    // Now match that list with the list of local transceivers that this peer wants added, and add the
                    // ones missing, while updating the transceiver info for existing ones, so that the answer that this
                    // peer will then send matches the reality of the existing Unity components.
                    var transceivers = _nativePeer.Transceivers;
                    int numNativeTransceivers = transceivers.Count;
                    int numExisting = Math.Min(numNativeTransceivers, _transceivers.Count);

                    // Associate registered media senders/receivers with existing transceivers
                    for (int i = 0; i < numExisting; ++i)
                    {
                        var transceiverInfo = _transceivers[i];
                        Transceiver tr = transceivers[i];
                        transceiverInfo.Transceiver = tr;

                        // If sending, fix up transceiver direction. Because the remote description was already applied before
                        // any sending transceiver was added (remember, the answering peer didn't add its media senders yet),
                        // all transceivers automatically added are in ReceiveOnly or Inactive state, since the implementation
                        // couldn't associate them with existing senders (none were added yet). This ensures that a TrackAdded
                        // event is fired on the remote peer when it receives the answer.
                        bool wantsSend = (transceiverInfo.Sender != null);
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

                            // If the offer doesn't allow to send (2 cases above) then this below will generate a renegotiation
                            // needed event, which will be temporarily delayed since we are in the middle of a negotiation already.
                            if (transceiverInfo.Kind == MediaKind.Audio)
                            {
                                var audioTransceiver = (AudioTransceiver)tr;
                                var audioSender = (AudioSender)transceiverInfo.Sender;
                                await audioSender.InitAndAttachTrackAsync(audioTransceiver);
                            }
                            else if (transceiverInfo.Kind == MediaKind.Video)
                            {
                                var videoTransceiver = (VideoTransceiver)tr;
                                var videoSender = (VideoSender)transceiverInfo.Sender;
                                await videoSender.InitAndAttachTrackAsync(videoTransceiver);
                            }
                        }

                        // If receiving, pair remote tracks just created by the offer description with the media receivers
                        bool wantsRecv = (transceiverInfo.Receiver != null);
                        if (wantsRecv)
                        {
                            if (transceiverInfo.Kind == MediaKind.Audio)
                            {
                                var audioTransceiver = (AudioTransceiver)tr;
                                if (audioTransceiver.RemoteTrack != null)
                                {
                                    var audioReceiver = (AudioReceiver)transceiverInfo.Receiver;
                                    audioReceiver.OnPaired(audioTransceiver.RemoteTrack);
                                }
                            }
                            else if (transceiverInfo.Kind == MediaKind.Video)
                            {
                                var videoTransceiver = (VideoTransceiver)tr;
                                if (videoTransceiver.RemoteTrack != null)
                                {
                                    var videoReceiver = (VideoReceiver)transceiverInfo.Receiver;
                                    videoReceiver.OnPaired(videoTransceiver.RemoteTrack);
                                }
                            }
                        }
                    }

                    // Ignore extra transceivers without a registered component to attach
                    for (int i = numExisting; i < numNativeTransceivers; ++i)
                    {
                        _mainThreadWorkQueue.Enqueue(() =>
                        {
                            Debug.LogWarning($"Peer connection {name} has transceiver #{i} but no sender/receveiver component to process it. The transceiver will be ignored.");
                        });
                    }
                }
                else if (type == "answer")
                {
                    // Much simpler, just pair the newly created remote tracks
                    var transceivers = _nativePeer.Transceivers;
                    int numNativeTransceivers = transceivers.Count;
                    int numExisting = Math.Min(numNativeTransceivers, _transceivers.Count);

                    // Associate registered media senders/receivers with existing transceivers
                    for (int i = 0; i < numExisting; ++i)
                    {
                        var transceiverInfo = _transceivers[i];
                        Transceiver tr = transceivers[i];
                        transceiverInfo.Transceiver = tr;

                        bool wantsRecv = (transceiverInfo.Receiver != null);
                        if (wantsRecv)
                        {
                            if (transceiverInfo.Kind == MediaKind.Audio)
                            {
                                var audioTransceiver = (AudioTransceiver)tr;
                                if (audioTransceiver.RemoteTrack != null)
                                {
                                    var audioReceiver = (AudioReceiver)transceiverInfo.Receiver;
                                    audioReceiver.OnPaired(audioTransceiver.RemoteTrack);
                                }
                            }
                            else if (transceiverInfo.Kind == MediaKind.Video)
                            {
                                var videoTransceiver = (VideoTransceiver)tr;
                                if (videoTransceiver.RemoteTrack != null)
                                {
                                    var videoReceiver = (VideoReceiver)transceiverInfo.Receiver;
                                    videoReceiver.OnPaired(videoTransceiver.RemoteTrack);
                                }
                            }
                        }
                    }

                    // Ignore extra transceivers without a registered component to attach
                    for (int i = numExisting; i < numNativeTransceivers; ++i)
                    {
                        _mainThreadWorkQueue.Enqueue(() =>
                        {
                            Debug.LogWarning($"Peer connection {name} has transceiver #{i} but no sender/receveiver component to process it. The transceiver will be ignored.");
                        });
                    }
                }
            });
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
            // Check in case InitializeAsync() was called first.
            if (_nativePeer == null)
            {
                CreateNativePeerConnection();
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
        /// Create a new native peer connection and register event handlers to it.
        /// This does not initialize the peer connection yet.
        /// </summary>
        private void CreateNativePeerConnection()
        {
            // Create the peer connection managed wrapper and its native implementation
            _nativePeer = new WebRTC.PeerConnection();

            // Register event handlers for remote tracks (media receivers)
            _nativePeer.AudioTrackAdded += Peer_AudioTrackAdded;
            _nativePeer.AudioTrackRemoved += Peer_AudioTrackRemoved;
            _nativePeer.VideoTrackAdded += Peer_VideoTrackAdded;
            _nativePeer.VideoTrackRemoved += Peer_VideoTrackRemoved;

            // Register event handlers for signaling
            if (Signaler != null)
            {
                _nativePeer.LocalSdpReadytoSend += Signaler_LocalSdpReadyToSend;
                _nativePeer.IceCandidateReadytoSend += Signaler_IceCandidateReadytoSend;
            }
        }

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
        /// Callback fired on the main Unity app thread once the WebRTC plugin was initialized successfully.
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

        private void Peer_AudioTrackAdded(RemoteAudioTrack track)
        {
            //for (int mlineIndex = 0; mlineIndex < _transceivers.Count; ++mlineIndex)
            //{
            //    var transceiverInfo = _transceivers[mlineIndex];
            //    if (transceiverInfo.Transceiver == track.Transceiver)
            //    {
            //        Debug.Assert(transceiverInfo.Kind == MediaKind.Audio);
            //        if (transceiverInfo.Receiver != null)
            //        {
            //            (transceiverInfo.Receiver as AudioReceiver).OnPaired(track);
            //        }
            //        else
            //        {
            //            Debug.LogWarning($"Received audio track {track.Name} for transceiver #{mlineIndex} but no audio receiver is registered to handle it. Ignoring that track.");
            //        }
            //    }
            //}
        }

        private void Peer_AudioTrackRemoved(RemoteAudioTrack track)
        {
            for (int mlineIndex = 0; mlineIndex < _transceivers.Count; ++mlineIndex)
            {
                var transceiverInfo = _transceivers[mlineIndex];
                if (transceiverInfo.Transceiver == track.Transceiver)
                {
                    Debug.Assert(transceiverInfo.Kind == MediaKind.Audio);
                    if (transceiverInfo.Receiver != null)
                    {
                        (transceiverInfo.Receiver as AudioReceiver).OnUnpaired(track);
                    }
                }
            }
        }

        private void Peer_VideoTrackAdded(RemoteVideoTrack track)
        {
            ////< FIXME - This can't work because transceivers have not been paired with components yet
            //for (int mlineIndex = 0; mlineIndex < _transceivers.Count; ++mlineIndex)
            //{
            //    var transceiverInfo = _transceivers[mlineIndex];
            //    if (transceiverInfo.Transceiver == track.Transceiver)
            //    {
            //        Debug.Assert(transceiverInfo.Kind == MediaKind.Video);
            //        if (transceiverInfo.Receiver != null)
            //        {
            //            (transceiverInfo.Receiver as VideoReceiver).OnPaired(track);
            //        }
            //        else
            //        {
            //            Debug.LogWarning($"Received video track {track.Name} for transceiver #{mlineIndex} but no video receiver is registered to handle it. Ignoring that track.");
            //        }
            //    }
            //}
        }

        private void Peer_VideoTrackRemoved(RemoteVideoTrack track)
        {
            for (int mlineIndex = 0; mlineIndex < _transceivers.Count; ++mlineIndex)
            {
                var transceiverInfo = _transceivers[mlineIndex];
                if (transceiverInfo.Transceiver == track.Transceiver)
                {
                    Debug.Assert(transceiverInfo.Kind == MediaKind.Video);
                    if (transceiverInfo.Receiver != null)
                    {
                        (transceiverInfo.Receiver as VideoReceiver).OnUnpaired(track);
                    }
                }
            }
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
                    await SetRemoteDescriptionAsync("offer", message.Data);
                    // If we get an offer, we immediately send an answer back
                    _nativePeer.CreateAnswer();
                    break;

                case Signaler.Message.WireMessageType.Answer:
                    await SetRemoteDescriptionAsync("answer", message.Data);
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

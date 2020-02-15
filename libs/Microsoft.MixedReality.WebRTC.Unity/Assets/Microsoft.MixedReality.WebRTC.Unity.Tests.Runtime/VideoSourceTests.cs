// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System.Collections;
using System.Threading;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.TestTools;

namespace Microsoft.MixedReality.WebRTC.Unity.Tests.Runtime
{
    public class VideoSourceTests
    {
        [SetUp]
        public void Setup()
        {
        }

        [TearDown]
        public void Shutdown()
        {
            // Force shutdown in case a test failure prevented cleaning-up some
            // native resources, thereby locking the native module and preventing
            // it from being unloaded/reloaded in the Unity editor.
            Library.ReportLiveObjects();
            //Library.ForceShutdown();
        }

        [UnityTest]
        public IEnumerator SingleOneWay() // S => R
        {
            // Create the peer connections
            var pc1_go = new GameObject("pc1");
            pc1_go.SetActive(false); // prevent auto-activation of components
            var pc1 = pc1_go.AddComponent<PeerConnection>();
            pc1.AutoInitializeOnStart = false;
            var pc2_go = new GameObject("pc2");
            pc2_go.SetActive(false); // prevent auto-activation of components
            var pc2 = pc2_go.AddComponent<PeerConnection>();
            pc2.AutoInitializeOnStart = false;

            // Create the signaler
            var sig_go = new GameObject("signaler");
            var sig = sig_go.AddComponent<HardcodedSignaler>();
            sig.Peer1 = pc1;
            sig.Peer2 = pc2;

            // Create the sender video source
            var sender1 = pc1_go.AddComponent<FakeVideoSource>();
            sender1.AutoAddTrack = true;
            sender1.TrackName = "track_name";
            TransceiverInfo tr1 = pc1.AddTransceiver(MediaKind.Video);
            tr1.Sender = sender1;

            // Create the receiver video source
            var receiver2 = pc2_go.AddComponent<VideoReceiver>();
            receiver2.AutoPlayOnPaired = true;
            TransceiverInfo tr2 = pc2.AddTransceiver(MediaKind.Video);
            tr2.Receiver = receiver2;

            // Activate
            pc1_go.SetActive(true);
            pc2_go.SetActive(true);

            // Initialize
            var initializedEvent1 = new ManualResetEventSlim(initialState: false);
            pc1.OnInitialized.AddListener(() => initializedEvent1.Set());
            Assert.IsNull(pc1.Peer);
            pc1.InitializeAsync().Wait(millisecondsTimeout: 50000);
            var initializedEvent2 = new ManualResetEventSlim(initialState: false);
            pc2.OnInitialized.AddListener(() => initializedEvent2.Set());
            Assert.IsNull(pc2.Peer);
            pc2.InitializeAsync().Wait(millisecondsTimeout: 50000);

            // Wait a frame so that the Unity event OnInitialized can propagate
            yield return null;

            // Check the event was raised
            Assert.IsTrue(initializedEvent1.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc1.Peer);
            Assert.IsTrue(initializedEvent2.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc2.Peer);

            // Confirm the sender is ready
            Assert.NotNull(sender1.Track);

            // Connect
            sig.Connect();

            // Check pairing
            Assert.NotNull(receiver2.Track);
        }

        [UnityTest]
        public IEnumerator SingleOneWayMissingRecvOffer() // S => _
        {
            // Create the peer connections
            var pc1_go = new GameObject("pc1");
            pc1_go.SetActive(false); // prevent auto-activation of components
            var pc1 = pc1_go.AddComponent<PeerConnection>();
            pc1.AutoInitializeOnStart = false;
            var pc2_go = new GameObject("pc2");
            pc2_go.SetActive(false); // prevent auto-activation of components
            var pc2 = pc2_go.AddComponent<PeerConnection>();
            pc2.AutoInitializeOnStart = false;

            // Create the signaler
            var sig_go = new GameObject("signaler");
            var sig = sig_go.AddComponent<HardcodedSignaler>();
            sig.Peer1 = pc1;
            sig.Peer2 = pc2;

            // Create the sender video source
            var sender1 = pc1_go.AddComponent<FakeVideoSource>();
            sender1.AutoAddTrack = true;
            sender1.TrackName = "track_name";
            TransceiverInfo tr1 = pc1.AddTransceiver(MediaKind.Video);
            tr1.Sender = sender1;

            // Missing video receiver on peer #2

            // Activate
            pc1_go.SetActive(true);
            pc2_go.SetActive(true);

            // Initialize
            var initializedEvent1 = new ManualResetEventSlim(initialState: false);
            pc1.OnInitialized.AddListener(() => initializedEvent1.Set());
            Assert.IsNull(pc1.Peer);
            pc1.InitializeAsync().Wait(millisecondsTimeout: 50000);
            var initializedEvent2 = new ManualResetEventSlim(initialState: false);
            pc2.OnInitialized.AddListener(() => initializedEvent2.Set());
            Assert.IsNull(pc2.Peer);
            pc2.InitializeAsync().Wait(millisecondsTimeout: 50000);

            // Wait a frame so that the Unity event OnInitialized can propagate
            yield return null;

            // Check the event was raised
            Assert.IsTrue(initializedEvent1.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc1.Peer);
            Assert.IsTrue(initializedEvent2.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc2.Peer);

            // Confirm the sender is ready
            Assert.NotNull(sender1.Track);

            // Connect
            sig.Connect();

            // Check that even though pairing failed lower-level remote track exists anyway
            Assert.AreEqual(1, pc1.Peer.LocalVideoTracks.Count);
            Assert.NotNull(sender1.Track);
            Assert.AreEqual(sender1.Track, pc1.Peer.LocalVideoTracks[0]);
            Assert.AreEqual(0, pc1.Peer.RemoteVideoTracks.Count);
            Assert.AreEqual(0, pc2.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(1, pc2.Peer.RemoteVideoTracks.Count);
            var remote2 = pc2.Peer.RemoteVideoTracks[0];
        }

        [UnityTest]
        public IEnumerator SingleOneWayMissingSenderOffer() // _ => R
        {
            // Create the peer connections
            var pc1_go = new GameObject("pc1");
            pc1_go.SetActive(false); // prevent auto-activation of components
            var pc1 = pc1_go.AddComponent<PeerConnection>();
            pc1.AutoInitializeOnStart = false;
            var pc2_go = new GameObject("pc2");
            pc2_go.SetActive(false); // prevent auto-activation of components
            var pc2 = pc2_go.AddComponent<PeerConnection>();
            pc2.AutoInitializeOnStart = false;

            // Create the signaler
            var sig_go = new GameObject("signaler");
            var sig = sig_go.AddComponent<HardcodedSignaler>();
            sig.Peer1 = pc1;
            sig.Peer2 = pc2;

            // Missing video sender on peer #1

            // Create the receiver video source
            var receiver2 = pc2_go.AddComponent<VideoReceiver>();
            receiver2.AutoPlayOnPaired = true;
            TransceiverInfo tr2 = pc2.AddTransceiver(MediaKind.Video);
            tr2.Receiver = receiver2;

            // Activate
            pc1_go.SetActive(true);
            pc2_go.SetActive(true);

            // Initialize
            var initializedEvent1 = new ManualResetEventSlim(initialState: false);
            pc1.OnInitialized.AddListener(() => initializedEvent1.Set());
            Assert.IsNull(pc1.Peer);
            pc1.InitializeAsync().Wait(millisecondsTimeout: 50000);
            var initializedEvent2 = new ManualResetEventSlim(initialState: false);
            pc2.OnInitialized.AddListener(() => initializedEvent2.Set());
            Assert.IsNull(pc2.Peer);
            pc2.InitializeAsync().Wait(millisecondsTimeout: 50000);

            // Wait a frame so that the Unity event OnInitialized can propagate
            yield return null;

            // Check the event was raised
            Assert.IsTrue(initializedEvent1.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc1.Peer);
            Assert.IsTrue(initializedEvent2.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc2.Peer);

            // Confirm the receiver track is not added yet, since remote tracks are only instantiated
            // as the result of a session negotiation.
            Assert.IsNull(receiver2.Track);

            // Connect
            sig.Connect();

            // Unlike in the case of a missing receiver, if there is no sender then nothing can be negotiated
            Assert.AreEqual(0, pc1.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc1.Peer.RemoteVideoTracks.Count);
            Assert.AreEqual(0, pc2.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc2.Peer.RemoteVideoTracks.Count);
            Assert.IsNull(receiver2.Track);
        }

        [UnityTest]
        public IEnumerator SingleTwoWayMissingSenderOffer() // R <= SR
        {
            // Create the peer connections
            var pc1_go = new GameObject("pc1");
            pc1_go.SetActive(false); // prevent auto-activation of components
            var pc1 = pc1_go.AddComponent<PeerConnection>();
            pc1.AutoInitializeOnStart = false;
            var pc2_go = new GameObject("pc2");
            pc2_go.SetActive(false); // prevent auto-activation of components
            var pc2 = pc2_go.AddComponent<PeerConnection>();
            pc2.AutoInitializeOnStart = false;

            // Create the signaler
            var sig_go = new GameObject("signaler");
            var sig = sig_go.AddComponent<HardcodedSignaler>();
            sig.Peer1 = pc1;
            sig.Peer2 = pc2;

            // Create the video sources on peer #1
            var receiver1 = pc1_go.AddComponent<VideoReceiver>();
            receiver1.AutoPlayOnPaired = true;
            TransceiverInfo tr1 = pc1.AddTransceiver(MediaKind.Video);
            tr1.Receiver = receiver1;

            // Create the video sources on peer #2
            var sender2 = pc2_go.AddComponent<FakeVideoSource>();
            sender2.AutoAddTrack = true;
            sender2.TrackName = "track_name";
            var receiver2 = pc2_go.AddComponent<VideoReceiver>();
            receiver2.AutoPlayOnPaired = true;
            TransceiverInfo tr2 = pc2.AddTransceiver(MediaKind.Video);
            tr2.Sender = sender2;
            tr2.Receiver = receiver2;

            // Activate
            pc1_go.SetActive(true);
            pc2_go.SetActive(true);

            // Initialize
            var initializedEvent1 = new ManualResetEventSlim(initialState: false);
            pc1.OnInitialized.AddListener(() => initializedEvent1.Set());
            Assert.IsNull(pc1.Peer);
            pc1.InitializeAsync().Wait(millisecondsTimeout: 50000);
            var initializedEvent2 = new ManualResetEventSlim(initialState: false);
            pc2.OnInitialized.AddListener(() => initializedEvent2.Set());
            Assert.IsNull(pc2.Peer);
            pc2.InitializeAsync().Wait(millisecondsTimeout: 50000);

            // Wait a frame so that the Unity event OnInitialized can propagate
            yield return null;

            // Check the event was raised
            Assert.IsTrue(initializedEvent1.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc1.Peer);
            Assert.IsTrue(initializedEvent2.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc2.Peer);

            // Confirm the senders are ready
            Assert.NotNull(sender2.Track);

            // Connect
            sig.Connect();

            // Check pairing
            Assert.AreEqual(0, pc1.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(1, pc1.Peer.RemoteVideoTracks.Count);
            Assert.AreEqual(1, pc2.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc2.Peer.RemoteVideoTracks.Count);
            Assert.NotNull(receiver1.Track);
        }

        [UnityTest]
        public IEnumerator SingleTwoWayMissingReceiverOffer() // SR <= S
        {
            // Create the peer connections
            var pc1_go = new GameObject("pc1");
            pc1_go.SetActive(false); // prevent auto-activation of components
            var pc1 = pc1_go.AddComponent<PeerConnection>();
            pc1.AutoInitializeOnStart = false;
            var pc2_go = new GameObject("pc2");
            pc2_go.SetActive(false); // prevent auto-activation of components
            var pc2 = pc2_go.AddComponent<PeerConnection>();
            pc2.AutoInitializeOnStart = false;

            // Create the signaler
            var sig_go = new GameObject("signaler");
            var sig = sig_go.AddComponent<HardcodedSignaler>();
            sig.Peer1 = pc1;
            sig.Peer2 = pc2;

            // Create the video sources on peer #1
            var sender1 = pc1_go.AddComponent<FakeVideoSource>();
            sender1.AutoAddTrack = true;
            sender1.TrackName = "track_name";
            var receiver1 = pc1_go.AddComponent<VideoReceiver>();
            receiver1.AutoPlayOnPaired = true;
            TransceiverInfo tr1 = pc1.AddTransceiver(MediaKind.Video);
            tr1.Sender = sender1;
            tr1.Receiver = receiver1;

            // Create the video sources on peer #2
            var sender2 = pc2_go.AddComponent<FakeVideoSource>();
            sender2.AutoAddTrack = true;
            sender2.TrackName = "track_name";
            TransceiverInfo tr2 = pc2.AddTransceiver(MediaKind.Video);
            tr2.Sender = sender2;

            // Activate
            pc1_go.SetActive(true);
            pc2_go.SetActive(true);

            // Initialize
            var initializedEvent1 = new ManualResetEventSlim(initialState: false);
            pc1.OnInitialized.AddListener(() => initializedEvent1.Set());
            Assert.IsNull(pc1.Peer);
            pc1.InitializeAsync().Wait(millisecondsTimeout: 50000);
            var initializedEvent2 = new ManualResetEventSlim(initialState: false);
            pc2.OnInitialized.AddListener(() => initializedEvent2.Set());
            Assert.IsNull(pc2.Peer);
            pc2.InitializeAsync().Wait(millisecondsTimeout: 50000);

            // Wait a frame so that the Unity event OnInitialized can propagate
            yield return null;

            // Check the event was raised
            Assert.IsTrue(initializedEvent1.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc1.Peer);
            Assert.IsTrue(initializedEvent2.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc2.Peer);

            // Confirm the senders are ready
            Assert.NotNull(sender1.Track);
            Assert.NotNull(sender2.Track);

            // Connect
            sig.Connect();

            // Check pairing
            Assert.AreEqual(1, pc1.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(1, pc1.Peer.RemoteVideoTracks.Count);
            Assert.AreEqual(1, pc2.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(1, pc2.Peer.RemoteVideoTracks.Count); // auto-create even if not paired
            Assert.NotNull(receiver1.Track);
        }

        [UnityTest]
        public IEnumerator SingleTwoWayMissingReceiverAnswer() // S => SR
        {
            // Create the peer connections
            var pc1_go = new GameObject("pc1");
            pc1_go.SetActive(false); // prevent auto-activation of components
            var pc1 = pc1_go.AddComponent<PeerConnection>();
            pc1.AutoInitializeOnStart = false;
            var pc2_go = new GameObject("pc2");
            pc2_go.SetActive(false); // prevent auto-activation of components
            var pc2 = pc2_go.AddComponent<PeerConnection>();
            pc2.AutoInitializeOnStart = false;

            // Create the signaler
            var sig_go = new GameObject("signaler");
            var sig = sig_go.AddComponent<HardcodedSignaler>();
            sig.Peer1 = pc1;
            sig.Peer2 = pc2;

            // Create the video sources on peer #1
            var sender1 = pc1_go.AddComponent<FakeVideoSource>();
            sender1.AutoAddTrack = true;
            sender1.TrackName = "track_name";
            TransceiverInfo tr1 = pc1.AddTransceiver(MediaKind.Video);
            tr1.Sender = sender1;

            // Create the video sources on peer #2
            var sender2 = pc2_go.AddComponent<FakeVideoSource>();
            sender2.AutoAddTrack = true;
            sender2.TrackName = "track_name";
            var receiver2 = pc2_go.AddComponent<VideoReceiver>();
            receiver2.AutoPlayOnPaired = true;
            TransceiverInfo tr2 = pc2.AddTransceiver(MediaKind.Video);
            tr2.Sender = sender2;
            tr2.Receiver = receiver2;

            // Activate
            pc1_go.SetActive(true);
            pc2_go.SetActive(true);

            // Initialize
            var initializedEvent1 = new ManualResetEventSlim(initialState: false);
            pc1.OnInitialized.AddListener(() => initializedEvent1.Set());
            Assert.IsNull(pc1.Peer);
            pc1.InitializeAsync().Wait(millisecondsTimeout: 50000);
            var initializedEvent2 = new ManualResetEventSlim(initialState: false);
            pc2.OnInitialized.AddListener(() => initializedEvent2.Set());
            Assert.IsNull(pc2.Peer);
            pc2.InitializeAsync().Wait(millisecondsTimeout: 50000);

            // Wait a frame so that the Unity event OnInitialized can propagate
            yield return null;

            // Check the event was raised
            Assert.IsTrue(initializedEvent1.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc1.Peer);
            Assert.IsTrue(initializedEvent2.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc2.Peer);

            // Confirm the senders are ready
            Assert.NotNull(sender1.Track);
            Assert.NotNull(sender2.Track);

            // Connect
            sig.Connect();

            // Check pairing
            Assert.AreEqual(1, pc1.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc1.Peer.RemoteVideoTracks.Count); // offer was SendOnly
            Assert.AreEqual(1, pc2.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(1, pc2.Peer.RemoteVideoTracks.Count);
            Assert.NotNull(receiver2.Track);
        }

        [UnityTest]
        public IEnumerator SingleTwoWayMissingSenderAnswer() // SR => R
        {
            // Create the peer connections
            var pc1_go = new GameObject("pc1");
            pc1_go.SetActive(false); // prevent auto-activation of components
            var pc1 = pc1_go.AddComponent<PeerConnection>();
            pc1.AutoInitializeOnStart = false;
            var pc2_go = new GameObject("pc2");
            pc2_go.SetActive(false); // prevent auto-activation of components
            var pc2 = pc2_go.AddComponent<PeerConnection>();
            pc2.AutoInitializeOnStart = false;

            // Create the signaler
            var sig_go = new GameObject("signaler");
            var sig = sig_go.AddComponent<HardcodedSignaler>();
            sig.Peer1 = pc1;
            sig.Peer2 = pc2;

            // Create the video sources on peer #1
            var sender1 = pc1_go.AddComponent<FakeVideoSource>();
            sender1.AutoAddTrack = true;
            sender1.TrackName = "track_name";
            var receiver1 = pc2_go.AddComponent<VideoReceiver>();
            receiver1.AutoPlayOnPaired = true;
            TransceiverInfo tr1 = pc1.AddTransceiver(MediaKind.Video);
            tr1.Sender = sender1;
            tr1.Receiver = receiver1;

            // Create the video sources on peer #2
            var receiver2 = pc2_go.AddComponent<VideoReceiver>();
            receiver2.AutoPlayOnPaired = true;
            TransceiverInfo tr2 = pc2.AddTransceiver(MediaKind.Video);
            tr2.Receiver = receiver2;

            // Activate
            pc1_go.SetActive(true);
            pc2_go.SetActive(true);

            // Initialize
            var initializedEvent1 = new ManualResetEventSlim(initialState: false);
            pc1.OnInitialized.AddListener(() => initializedEvent1.Set());
            Assert.IsNull(pc1.Peer);
            pc1.InitializeAsync().Wait(millisecondsTimeout: 50000);
            var initializedEvent2 = new ManualResetEventSlim(initialState: false);
            pc2.OnInitialized.AddListener(() => initializedEvent2.Set());
            Assert.IsNull(pc2.Peer);
            pc2.InitializeAsync().Wait(millisecondsTimeout: 50000);

            // Wait a frame so that the Unity event OnInitialized can propagate
            yield return null;

            // Check the event was raised
            Assert.IsTrue(initializedEvent1.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc1.Peer);
            Assert.IsTrue(initializedEvent2.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc2.Peer);

            // Confirm the senders are ready
            Assert.NotNull(sender1.Track);

            // Connect
            sig.Connect();

            // Check pairing
            Assert.AreEqual(1, pc1.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc1.Peer.RemoteVideoTracks.Count); // answer was RecvOnly
            Assert.AreEqual(0, pc2.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(1, pc2.Peer.RemoteVideoTracks.Count);
            Assert.NotNull(receiver2.Track);
        }

        [UnityTest]
        public IEnumerator SingleTwoWay()
        {
            // Create the peer connections
            var pc1_go = new GameObject("pc1");
            pc1_go.SetActive(false); // prevent auto-activation of components
            var pc1 = pc1_go.AddComponent<PeerConnection>();
            pc1.AutoInitializeOnStart = false;
            var pc2_go = new GameObject("pc2");
            pc2_go.SetActive(false); // prevent auto-activation of components
            var pc2 = pc2_go.AddComponent<PeerConnection>();
            pc2.AutoInitializeOnStart = false;

            // Create the signaler
            var sig_go = new GameObject("signaler");
            var sig = sig_go.AddComponent<HardcodedSignaler>();
            sig.Peer1 = pc1;
            sig.Peer2 = pc2;

            // Create the video sources on peer #1
            var sender1 = pc1_go.AddComponent<FakeVideoSource>();
            sender1.AutoAddTrack = true;
            sender1.TrackName = "track_name";
            var receiver1 = pc1_go.AddComponent<VideoReceiver>();
            receiver1.AutoPlayOnPaired = true;
            TransceiverInfo tr1 = pc1.AddTransceiver(MediaKind.Video);
            tr1.Sender = sender1;
            tr1.Receiver = receiver1;

            // Create the video sources on peer #2
            var sender2 = pc2_go.AddComponent<FakeVideoSource>();
            sender2.AutoAddTrack = true;
            sender2.TrackName = "track_name";
            var receiver2 = pc2_go.AddComponent<VideoReceiver>();
            receiver2.AutoPlayOnPaired = true;
            TransceiverInfo tr2 = pc2.AddTransceiver(MediaKind.Video);
            tr2.Sender = sender2;
            tr2.Receiver = receiver2;

            // Activate
            pc1_go.SetActive(true);
            pc2_go.SetActive(true);

            // Initialize
            var initializedEvent1 = new ManualResetEventSlim(initialState: false);
            pc1.OnInitialized.AddListener(() => initializedEvent1.Set());
            Assert.IsNull(pc1.Peer);
            pc1.InitializeAsync().Wait(millisecondsTimeout: 50000);
            var initializedEvent2 = new ManualResetEventSlim(initialState: false);
            pc2.OnInitialized.AddListener(() => initializedEvent2.Set());
            Assert.IsNull(pc2.Peer);
            pc2.InitializeAsync().Wait(millisecondsTimeout: 50000);

            // Wait a frame so that the Unity event OnInitialized can propagate
            yield return null;

            // Check the event was raised
            Assert.IsTrue(initializedEvent1.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc1.Peer);
            Assert.IsTrue(initializedEvent2.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc2.Peer);

            // Confirm the senders are ready
            Assert.NotNull(sender1.Track);
            Assert.NotNull(sender2.Track);

            // Connect
            sig.Connect();

            // Check pairing
            Assert.NotNull(receiver1.Track);
            Assert.NotNull(receiver2.Track);
        }

        class PeerConfig
        {
            // Input
            public Transceiver.Direction dir;
            public FakeVideoSource sender;
            public VideoReceiver receiver;

            // Output
            public bool expectSender;
            public bool expectReceiver;
        }

        class MultiConfig
        {
            public PeerConfig peer1;
            public PeerConfig peer2;
        };

        private bool HasSend(Transceiver.Direction dir)
        {
            return ((dir == Transceiver.Direction.SendOnly) || (dir == Transceiver.Direction.SendReceive));
        }

        private bool HasRecv(Transceiver.Direction dir)
        {
            return ((dir == Transceiver.Direction.ReceiveOnly) || (dir == Transceiver.Direction.SendReceive));
        }

        [UnityTest]
        public IEnumerator Multi()
        {
            // Create the peer connections
            var pc1_go = new GameObject("pc1");
            pc1_go.SetActive(false); // prevent auto-activation of components
            var pc1 = pc1_go.AddComponent<PeerConnection>();
            pc1.AutoInitializeOnStart = false;
            var pc2_go = new GameObject("pc2");
            pc2_go.SetActive(false); // prevent auto-activation of components
            var pc2 = pc2_go.AddComponent<PeerConnection>();
            pc2.AutoInitializeOnStart = false;

            // Create the signaler
            var sig_go = new GameObject("signaler");
            var sig = sig_go.AddComponent<HardcodedSignaler>();
            sig.Peer1 = pc1;
            sig.Peer2 = pc2;

            // Create the senders and receivers
            //     P1     P2
            // 0 : S   =>  R
            // 1 : SR <=> SR
            // 2 : S   => SR
            // 3 :  R <=  SR
            // 4 : S   =>  R

            const int NumTransceivers = 5;

            // P1 has 4 senders added to it
            int numLocal1 = 4;

            // P1 receives 2 tracks from the 3 P2 senders (one is refused)
            int numRemote1 = 2;

            // P2 has 3 senders added to it
            int numLocal2 = 3;

            // P2 receives 4 tracks from the 4 P1 senders
            int numRemote2 = 4;

            var cfgs = new MultiConfig[NumTransceivers]
            {
                new MultiConfig {
                    peer1 = new PeerConfig {
                        dir = Transceiver.Direction.SendOnly,
                        expectSender = true,
                        expectReceiver = false,
                    },
                    peer2 = new PeerConfig {
                        dir = Transceiver.Direction.ReceiveOnly,
                        expectSender = false,
                        expectReceiver = true,
                    }
                },
                new MultiConfig {
                    peer1 = new PeerConfig {
                        dir = Transceiver.Direction.SendReceive,
                        expectSender = true,
                        expectReceiver = true,
                    },
                    peer2 = new PeerConfig {
                        dir = Transceiver.Direction.SendReceive,
                        expectSender = true,
                        expectReceiver = true,
                    },
                },
                new MultiConfig {
                    peer1 = new PeerConfig {
                        dir = Transceiver.Direction.SendOnly,
                        expectSender = true,
                        expectReceiver = false,
                    },
                    peer2 = new PeerConfig {
                        dir = Transceiver.Direction.SendReceive,
                        expectSender = true,
                        expectReceiver = true,
                    },
                },
                new MultiConfig {
                    peer1 = new PeerConfig {
                        dir = Transceiver.Direction.ReceiveOnly,
                        expectSender = false,
                        expectReceiver = true,
                    },
                    peer2 = new PeerConfig {
                        dir = Transceiver.Direction.SendReceive,
                        expectSender = true,
                        expectReceiver = false,
                    },
                },
                new MultiConfig {
                    peer1 = new PeerConfig {
                        dir = Transceiver.Direction.SendOnly,
                        expectSender = true,
                        expectReceiver = false,
                    },
                    peer2 = new PeerConfig {
                        dir = Transceiver.Direction.ReceiveOnly,
                        expectSender = false,
                        expectReceiver = true,
                    },
                },
            };
            for (int i = 0; i < NumTransceivers; ++i)
            {
                var cfg = cfgs[i];

                {
                    TransceiverInfo tr1 = pc1.AddTransceiver(MediaKind.Video);
                    var peer = cfg.peer1;
                    if (HasSend(peer.dir))
                    {
                        var sender1 = pc1_go.AddComponent<FakeVideoSource>();
                        sender1.AutoAddTrack = true;
                        sender1.TrackName = $"track{i}";
                        peer.sender = sender1;
                        tr1.Sender = sender1;
                    }
                    if (HasRecv(peer.dir))
                    {
                        var receiver1 = pc1_go.AddComponent<VideoReceiver>();
                        receiver1.AutoPlayOnPaired = true;
                        peer.receiver = receiver1;
                        tr1.Receiver = receiver1;
                    }
                }

                {
                    TransceiverInfo tr2 = pc2.AddTransceiver(MediaKind.Video);
                    var peer = cfg.peer2;
                    if (HasSend(peer.dir))
                    {
                        var sender2 = pc2_go.AddComponent<FakeVideoSource>();
                        sender2.AutoAddTrack = true;
                        sender2.TrackName = $"track{i}";
                        peer.sender = sender2;
                        tr2.Sender = sender2;
                    }
                    if (HasRecv(peer.dir))
                    {
                        var receiver2 = pc2_go.AddComponent<VideoReceiver>();
                        receiver2.AutoPlayOnPaired = true;
                        peer.receiver = receiver2;
                        tr2.Receiver = receiver2;
                    }
                }
            }

            // Activate
            pc1_go.SetActive(true);
            pc2_go.SetActive(true);

            // Initialize
            var initializedEvent1 = new ManualResetEventSlim(initialState: false);
            pc1.OnInitialized.AddListener(() => initializedEvent1.Set());
            Assert.IsNull(pc1.Peer);
            pc1.InitializeAsync().Wait(millisecondsTimeout: 50000);
            var initializedEvent2 = new ManualResetEventSlim(initialState: false);
            pc2.OnInitialized.AddListener(() => initializedEvent2.Set());
            Assert.IsNull(pc2.Peer);
            pc2.InitializeAsync().Wait(millisecondsTimeout: 50000);

            // Wait a frame so that the Unity event OnInitialized can propagate
            yield return null;

            // Check the event was raised
            Assert.IsTrue(initializedEvent1.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc1.Peer);
            Assert.IsTrue(initializedEvent2.Wait(millisecondsTimeout: 50000));
            Assert.IsNotNull(pc2.Peer);

            // Confirm the senders are ready
            for (int i = 0; i < NumTransceivers; ++i)
            {
                var cfg = cfgs[i];
                if (cfg.peer1.expectSender)
                {
                    Assert.NotNull(cfg.peer1.sender.Track, $"Transceiver #{i} missing local track on Peer #1");
                }
                if (cfg.peer2.expectSender)
                {
                    Assert.NotNull(cfg.peer2.sender.Track, $"Transceiver #{i} missing local track on Peer #2");
                }
            }

            // Connect
            sig.Connect();

            // Check pairing
            Assert.AreEqual(numLocal1, pc1.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(numRemote1, pc1.Peer.RemoteVideoTracks.Count);
            Assert.AreEqual(numLocal2, pc2.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(numRemote2, pc2.Peer.RemoteVideoTracks.Count);
            for (int i = 0; i < NumTransceivers; ++i)
            {
                var cfg = cfgs[i];
                if (cfg.peer1.expectReceiver)
                {
                    Assert.NotNull(cfg.peer1.receiver.Track, $"Transceiver #{i} missing remote track on Peer #1");
                }
                if (cfg.peer2.expectReceiver)
                {
                    Assert.NotNull(cfg.peer2.receiver.Track, $"Transceiver #{i} Missing remote track on Peer #2");
                }
            }
        }
    }
}

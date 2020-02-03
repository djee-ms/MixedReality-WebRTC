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
            sender1.PeerConnection = pc1;
            sender1.TrackName = "track_name";

            // Create the receiver video source
            var receiver2 = pc2_go.AddComponent<VideoReceiver>();
            receiver2.AutoPlayOnAdded = true;
            receiver2.PeerConnection = pc2;

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
            sender1.PeerConnection = pc1;
            sender1.TrackName = "track_name";

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

            // Even though the source components were not paired, the low-level imlementation generated
            // a transceiver with the name negotiated from the offer, which is the stream name. This means
            // a remote source could be paired after the fact from now.
            Assert.AreEqual(sender1.TrackName, remote2.Transceiver.Name);
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
            receiver2.AutoPlayOnAdded = true;
            receiver2.PeerConnection = pc2;

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

        // NOTE : the test below is disabled because of a bug in the Unified Plan implementation of Google,
        //        which forgets to set the stream IDs on the SDP offer if there is no sender and only a receiver.

        //[UnityTest]
        //public IEnumerator SingleTwoWayMissingSenderOffer() // R <= SR
        //{
        //    // Create the peer connections
        //    var pc1_go = new GameObject("pc1");
        //    pc1_go.SetActive(false); // prevent auto-activation of components
        //    var pc1 = pc1_go.AddComponent<PeerConnection>();
        //    pc1.AutoInitializeOnStart = false;
        //    var pc2_go = new GameObject("pc2");
        //    pc2_go.SetActive(false); // prevent auto-activation of components
        //    var pc2 = pc2_go.AddComponent<PeerConnection>();
        //    pc2.AutoInitializeOnStart = false;

        //    // Create the signaler
        //    var sig_go = new GameObject("signaler");
        //    var sig = sig_go.AddComponent<HardcodedSignaler>();
        //    sig.Peer1 = pc1;
        //    sig.Peer2 = pc2;

        //    // Create the video sources on peer #1
        //    var receiver1 = pc1_go.AddComponent<VideoReceiver>();
        //    receiver1.AutoPlayOnAdded = true;
        //    receiver1.PeerConnection = pc1;
        //    receiver1.TargetTransceiverName = "track_name";

        //    // Create the video sources on peer #2
        //    var sender2 = pc2_go.AddComponent<FakeVideoSource>();
        //    sender2.AutoAddTrack = true;
        //    sender2.PeerConnection = pc2;
        //    sender2.TrackName = "track_name";
        //    var receiver2 = pc2_go.AddComponent<VideoReceiver>();
        //    receiver2.AutoPlayOnAdded = true;
        //    receiver2.PeerConnection = pc2;
        //    receiver2.TargetTransceiverName = "track_name";

        //    // Activate
        //    pc1_go.SetActive(true);
        //    pc2_go.SetActive(true);

        //    // Initialize
        //    var initializedEvent1 = new ManualResetEventSlim(initialState: false);
        //    pc1.OnInitialized.AddListener(() => initializedEvent1.Set());
        //    Assert.IsNull(pc1.Peer);
        //    pc1.InitializeAsync().Wait(millisecondsTimeout: 50000);
        //    var initializedEvent2 = new ManualResetEventSlim(initialState: false);
        //    pc2.OnInitialized.AddListener(() => initializedEvent2.Set());
        //    Assert.IsNull(pc2.Peer);
        //    pc2.InitializeAsync().Wait(millisecondsTimeout: 50000);

        //    // Wait a frame so that the Unity event OnInitialized can propagate
        //    yield return null;

        //    // Check the event was raised
        //    Assert.IsTrue(initializedEvent1.Wait(millisecondsTimeout: 50000));
        //    Assert.IsNotNull(pc1.Peer);
        //    Assert.IsTrue(initializedEvent2.Wait(millisecondsTimeout: 50000));
        //    Assert.IsNotNull(pc2.Peer);

        //    // Confirm the senders are ready
        //    Assert.NotNull(sender2.Track);

        //    // Connect
        //    sig.Connect();

        //    // Check pairing
        //    Assert.AreEqual(0, pc1.Peer.LocalVideoTracks.Count);
        //    Assert.AreEqual(1, pc1.Peer.RemoteVideoTracks.Count);
        //    Assert.AreEqual(1, pc2.Peer.LocalVideoTracks.Count);
        //    Assert.AreEqual(0, pc2.Peer.RemoteVideoTracks.Count);
        //    Assert.NotNull(receiver2.Track);
        //}

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
            sender1.PeerConnection = pc1;
            sender1.TrackName = "track_name";
            var receiver1 = pc1_go.AddComponent<VideoReceiver>();
            receiver1.AutoPlayOnAdded = true;
            receiver1.PeerConnection = pc1;

            // Create the video sources on peer #2
            var sender2 = pc2_go.AddComponent<FakeVideoSource>();
            sender2.AutoAddTrack = true;
            sender2.PeerConnection = pc2;
            sender2.TrackName = "track_name";

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
            sender1.PeerConnection = pc1;
            sender1.TrackName = "track_name";

            // Create the video sources on peer #2
            var sender2 = pc2_go.AddComponent<FakeVideoSource>();
            sender2.AutoAddTrack = true;
            sender2.PeerConnection = pc2;
            sender2.TrackName = "track_name";
            var receiver2 = pc2_go.AddComponent<VideoReceiver>();
            receiver2.AutoPlayOnAdded = true;
            receiver2.PeerConnection = pc2;

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
            sender1.PeerConnection = pc1;
            sender1.TrackName = "track_name";
            var receiver1 = pc2_go.AddComponent<VideoReceiver>();
            receiver1.AutoPlayOnAdded = true;
            receiver1.PeerConnection = pc1;

            // Create the video sources on peer #2
            var receiver2 = pc2_go.AddComponent<VideoReceiver>();
            receiver2.AutoPlayOnAdded = true;
            receiver2.PeerConnection = pc2;

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

            // Create the senders
            var sender1 = pc1_go.AddComponent<FakeVideoSource>();
            sender1.AutoAddTrack = true;
            sender1.PeerConnection = pc1;
            sender1.TrackName = "track_name";
            var sender2 = pc2_go.AddComponent<FakeVideoSource>();
            sender2.AutoAddTrack = true;
            sender2.PeerConnection = pc2;
            sender2.TrackName = "track_name";

            // Create the receivers
            var receiver1 = pc1_go.AddComponent<VideoReceiver>();
            receiver1.AutoPlayOnAdded = true;
            receiver1.PeerConnection = pc1;
            var receiver2 = pc2_go.AddComponent<VideoReceiver>();
            receiver2.AutoPlayOnAdded = true;
            receiver2.PeerConnection = pc2;

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
        class Config
        {
            public Transceiver.Direction dir1;
            public Transceiver.Direction dir2;
            public FakeVideoSource sender1;
            public FakeVideoSource sender2;
            public VideoReceiver receiver1;
            public VideoReceiver receiver2;
            public int numSender1;
            public int numSender2;
            public int numReceiver1;
            public int numReceiver2;
        };

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
            // 3 :  R <=  SR  -- bug in Google's implementation, won't work
            // 4 : S   =>  R

            // P1 has 4 senders added to it
            int numLocal1 = 4;

            // P1 receives 2 tracks, but one of which won't work because
            // of a bug in the Google implementation
            // https://webrtc.googlesource.com/src/+/5c72e71e14cfa76a2d1b0979d6b918abe187c208^!/
            int numRemote1 = 1; // 2; see bug above

            // P2 has 3 senders added to it
            int numLocal2 = 3;

            // P2 receives 4 tracks from the 4 P1 senders
            int numRemote2 = 4;

            var cfgs = new Config[5]
            {
                new Config {
                    dir1 = Transceiver.Direction.SendOnly,
                    dir2 = Transceiver.Direction.ReceiveOnly,
                    numSender1 = 1,
                    numSender2 = 0,
                    numReceiver1 = 0,
                    numReceiver2 = 1,
                },
                new Config {
                    dir1 = Transceiver.Direction.SendReceive,
                    dir2 = Transceiver.Direction.SendReceive,
                    numSender1 = 1,
                    numSender2 = 1,
                    numReceiver1 = 1,
                    numReceiver2 = 1,
                },
                new Config {
                    dir1 = Transceiver.Direction.SendOnly,
                    dir2 = Transceiver.Direction.SendReceive,
                    numSender1 = 1,
                    numSender2 = 1,
                    numReceiver1 = 0,
                    numReceiver2 = 1,
                },
                new Config {
                    dir1 = Transceiver.Direction.ReceiveOnly,
                    dir2 = Transceiver.Direction.SendReceive,
                    numSender1 = 0,
                    numSender2 = 1,
                    numReceiver1 = 0, // 1; see bug above
                    numReceiver2 = 0,
                },
                new Config {
                    dir1 = Transceiver.Direction.SendOnly,
                    dir2 = Transceiver.Direction.ReceiveOnly,
                    numSender1 = 1,
                    numSender2 = 0,
                    numReceiver1 = 0,
                    numReceiver2 = 1,
                },
            };
            for (int i = 0; i < 5; ++i)
            {
                var cfg = cfgs[i];
                if ((cfg.dir1 == Transceiver.Direction.SendOnly) || (cfg.dir1 == Transceiver.Direction.SendReceive))
                {
                    var sender1 = pc1_go.AddComponent<FakeVideoSource>();
                    sender1.AutoAddTrack = true;
                    sender1.PeerConnection = pc1;
                    sender1.TrackName = $"track{i}";
                    cfg.sender1 = sender1;
                }
                if ((cfg.dir1 == Transceiver.Direction.ReceiveOnly) || (cfg.dir1 == Transceiver.Direction.SendReceive))
                {
                    var receiver1 = pc1_go.AddComponent<VideoReceiver>();
                    receiver1.AutoPlayOnAdded = true;
                    receiver1.PeerConnection = pc1;
                    cfg.receiver1 = receiver1;
                }
                if ((cfg.dir2 == Transceiver.Direction.SendOnly) || (cfg.dir2 == Transceiver.Direction.SendReceive))
                {
                    var sender2 = pc2_go.AddComponent<FakeVideoSource>();
                    sender2.AutoAddTrack = true;
                    sender2.PeerConnection = pc2;
                    sender2.TrackName = "track_name";
                    sender2.TrackName = $"track{i}";
                    cfg.sender2 = sender2;
                }
                if ((cfg.dir2 == Transceiver.Direction.ReceiveOnly) || (cfg.dir2 == Transceiver.Direction.SendReceive))
                {
                    var receiver2 = pc2_go.AddComponent<VideoReceiver>();
                    receiver2.AutoPlayOnAdded = true;
                    receiver2.PeerConnection = pc2;
                    cfg.receiver2 = receiver2;
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
            foreach (var cfg in cfgs)
            {
                if (cfg.numSender1 > 0)
                {
                    Assert.NotNull(cfg.sender1.Track);
                }
                if (cfg.numSender2 > 0)
                {
                    Assert.NotNull(cfg.sender2.Track);
                }
            }

            // Connect
            sig.Connect();

            // Check pairing
            Assert.AreEqual(numLocal1, pc1.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(numRemote1, pc1.Peer.RemoteVideoTracks.Count);
            Assert.AreEqual(numLocal2, pc2.Peer.LocalVideoTracks.Count);
            Assert.AreEqual(numRemote2, pc2.Peer.RemoteVideoTracks.Count);
            foreach (var cfg in cfgs)
            {
                if (cfg.numReceiver1 > 0)
                {
                    Assert.NotNull(cfg.receiver1.Track);
                }
                if (cfg.numReceiver2 > 0)
                {
                    Assert.NotNull(cfg.receiver2.Track);
                }
            }
        }
    }
}

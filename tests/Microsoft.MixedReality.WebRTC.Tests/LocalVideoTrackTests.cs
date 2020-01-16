// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

using System;
using System.Threading;
using System.Threading.Tasks;
using NUnit.Framework;
using NUnit.Framework.Internal;

namespace Microsoft.MixedReality.WebRTC.Tests
{
    [TestFixture]
    internal class LocalVideoTrackTests
    {
        PeerConnection pc1_ = null;
        PeerConnection pc2_ = null;
        ManualResetEventSlim connectedEvent1_ = null;
        ManualResetEventSlim connectedEvent2_ = null;
        ManualResetEventSlim renegotiationEvent1_ = null;
        ManualResetEventSlim renegotiationEvent2_ = null;
        ManualResetEventSlim audioTrackAddedEvent1_ = null;
        ManualResetEventSlim audioTrackAddedEvent2_ = null;
        ManualResetEventSlim audioTrackRemovedEvent1_ = null;
        ManualResetEventSlim audioTrackRemovedEvent2_ = null;
        ManualResetEventSlim videoTrackAddedEvent1_ = null;
        ManualResetEventSlim videoTrackAddedEvent2_ = null;
        ManualResetEventSlim videoTrackRemovedEvent1_ = null;
        ManualResetEventSlim videoTrackRemovedEvent2_ = null;
        ManualResetEventSlim iceConnectedEvent1_ = null;
        ManualResetEventSlim iceConnectedEvent2_ = null;
        bool suspendOffer1_ = false;
        bool suspendOffer2_ = false;

        [SetUp]
        public void SetupConnection()
        {
            // Create the 2 peers
            var config = new PeerConnectionConfiguration();
            pc1_ = new PeerConnection();
            pc2_ = new PeerConnection();
            pc1_.InitializeAsync(config).Wait(); // cannot use async/await in OneTimeSetUp
            pc2_.InitializeAsync(config).Wait();

            // Allocate callback events
            connectedEvent1_ = new ManualResetEventSlim(false);
            connectedEvent2_ = new ManualResetEventSlim(false);
            iceConnectedEvent1_ = new ManualResetEventSlim(false);
            iceConnectedEvent2_ = new ManualResetEventSlim(false);
            renegotiationEvent1_ = new ManualResetEventSlim(false);
            renegotiationEvent2_ = new ManualResetEventSlim(false);
            audioTrackAddedEvent1_ = new ManualResetEventSlim(false);
            audioTrackAddedEvent2_ = new ManualResetEventSlim(false);
            audioTrackRemovedEvent1_ = new ManualResetEventSlim(false);
            audioTrackRemovedEvent2_ = new ManualResetEventSlim(false);
            videoTrackAddedEvent1_ = new ManualResetEventSlim(false);
            videoTrackAddedEvent2_ = new ManualResetEventSlim(false);
            videoTrackRemovedEvent1_ = new ManualResetEventSlim(false);
            videoTrackRemovedEvent2_ = new ManualResetEventSlim(false);

            // Connect all signals
            pc1_.Connected += OnConnected1;
            pc2_.Connected += OnConnected2;
            pc1_.LocalSdpReadytoSend += OnLocalSdpReady1;
            pc2_.LocalSdpReadytoSend += OnLocalSdpReady2;
            pc1_.IceCandidateReadytoSend += OnIceCandidateReadytoSend1;
            pc2_.IceCandidateReadytoSend += OnIceCandidateReadytoSend2;
            pc1_.IceStateChanged += OnIceStateChanged1;
            pc2_.IceStateChanged += OnIceStateChanged2;
            pc1_.RenegotiationNeeded += OnRenegotiationNeeded1;
            pc2_.RenegotiationNeeded += OnRenegotiationNeeded2;
            pc1_.AudioTrackAdded += OnAudioTrackAdded1;
            pc2_.AudioTrackAdded += OnAudioTrackAdded2;
            pc1_.AudioTrackRemoved += OnAudioTrackRemoved1;
            pc2_.AudioTrackRemoved += OnAudioTrackRemoved2;
            pc1_.VideoTrackAdded += OnVideoTrackAdded1;
            pc2_.VideoTrackAdded += OnVideoTrackAdded2;
            pc1_.VideoTrackRemoved += OnVideoTrackRemoved1;
            pc2_.VideoTrackRemoved += OnVideoTrackRemoved2;
        }

        [TearDown]
        public void TearDownConnectin()
        {
            // Unregister all callbacks
            pc1_.LocalSdpReadytoSend -= OnLocalSdpReady1;
            pc2_.LocalSdpReadytoSend -= OnLocalSdpReady2;
            pc1_.IceCandidateReadytoSend -= OnIceCandidateReadytoSend1;
            pc2_.IceCandidateReadytoSend -= OnIceCandidateReadytoSend2;
            pc1_.IceStateChanged -= OnIceStateChanged1;
            pc2_.IceStateChanged -= OnIceStateChanged2;
            pc1_.RenegotiationNeeded -= OnRenegotiationNeeded1;
            pc2_.RenegotiationNeeded -= OnRenegotiationNeeded2;
            pc1_.AudioTrackAdded -= OnAudioTrackAdded1;
            pc2_.AudioTrackAdded -= OnAudioTrackAdded2;
            pc1_.AudioTrackRemoved -= OnAudioTrackRemoved1;
            pc2_.AudioTrackRemoved -= OnAudioTrackRemoved2;
            pc1_.VideoTrackAdded -= OnVideoTrackAdded1;
            pc2_.VideoTrackAdded -= OnVideoTrackAdded2;
            pc1_.VideoTrackRemoved -= OnVideoTrackRemoved1;
            pc2_.VideoTrackRemoved -= OnVideoTrackRemoved2;

            // Clean-up callback events
            audioTrackAddedEvent1_.Dispose();
            audioTrackAddedEvent1_ = null;
            audioTrackRemovedEvent1_.Dispose();
            audioTrackRemovedEvent1_ = null;
            audioTrackAddedEvent2_.Dispose();
            audioTrackAddedEvent2_ = null;
            audioTrackRemovedEvent2_.Dispose();
            audioTrackRemovedEvent2_ = null;
            videoTrackAddedEvent1_.Dispose();
            videoTrackAddedEvent1_ = null;
            videoTrackRemovedEvent1_.Dispose();
            videoTrackRemovedEvent1_ = null;
            videoTrackAddedEvent2_.Dispose();
            videoTrackAddedEvent2_ = null;
            videoTrackRemovedEvent2_.Dispose();
            videoTrackRemovedEvent2_ = null;
            renegotiationEvent1_.Dispose();
            renegotiationEvent1_ = null;
            renegotiationEvent2_.Dispose();
            renegotiationEvent2_ = null;
            iceConnectedEvent1_.Dispose();
            iceConnectedEvent1_ = null;
            iceConnectedEvent2_.Dispose();
            iceConnectedEvent2_ = null;
            connectedEvent1_.Dispose();
            connectedEvent1_ = null;
            connectedEvent2_.Dispose();
            connectedEvent2_ = null;

            // Destroy peers
            pc1_.Close();
            pc1_.Dispose();
            pc1_ = null;
            pc2_.Close();
            pc2_.Dispose();
            pc2_ = null;
        }

        private void OnConnected1()
        {
            connectedEvent1_.Set();
        }

        private void OnConnected2()
        {
            connectedEvent2_.Set();
        }

        private void OnLocalSdpReady1(string type, string sdp)
        {
            pc2_.SetRemoteDescription(type, sdp);
            if (type == "offer")
            {
                pc2_.CreateAnswer();
            }
        }

        private void OnLocalSdpReady2(string type, string sdp)
        {
            pc1_.SetRemoteDescription(type, sdp);
            if (type == "offer")
            {
                pc1_.CreateAnswer();
            }
        }

        private void OnIceCandidateReadytoSend1(string candidate, int sdpMlineindex, string sdpMid)
        {
            pc2_.AddIceCandidate(sdpMid, sdpMlineindex, candidate);
        }

        private void OnIceCandidateReadytoSend2(string candidate, int sdpMlineindex, string sdpMid)
        {
            pc1_.AddIceCandidate(sdpMid, sdpMlineindex, candidate);
        }

        private void OnRenegotiationNeeded1()
        {
            renegotiationEvent1_.Set();
            if (pc1_.IsConnected && !suspendOffer1_)
            {
                pc1_.CreateOffer();
            }
        }

        private void OnRenegotiationNeeded2()
        {
            renegotiationEvent2_.Set();
            if (pc2_.IsConnected && !suspendOffer2_)
            {
                pc2_.CreateOffer();
            }
        }

        private void OnAudioTrackAdded1(RemoteAudioTrack track)
        {
            audioTrackAddedEvent1_.Set();
        }

        private void OnAudioTrackAdded2(RemoteAudioTrack track)
        {
            audioTrackAddedEvent2_.Set();
        }

        private void OnAudioTrackRemoved1(RemoteAudioTrack track)
        {
            audioTrackRemovedEvent1_.Set();
        }

        private void OnAudioTrackRemoved2(RemoteAudioTrack track)
        {
            audioTrackRemovedEvent2_.Set();
        }

        private void OnVideoTrackAdded1(RemoteVideoTrack track)
        {
            videoTrackAddedEvent1_.Set();
        }

        private void OnVideoTrackAdded2(RemoteVideoTrack track)
        {
            videoTrackAddedEvent2_.Set();
        }

        private void OnVideoTrackRemoved1(RemoteVideoTrack track)
        {
            videoTrackRemovedEvent1_.Set();
        }

        private void OnVideoTrackRemoved2(RemoteVideoTrack track)
        {
            videoTrackRemovedEvent2_.Set();
        }

        private void OnIceStateChanged1(IceConnectionState newState)
        {
            if ((newState == IceConnectionState.Connected) || (newState == IceConnectionState.Completed))
            {
                iceConnectedEvent1_.Set();
            }
        }

        private void OnIceStateChanged2(IceConnectionState newState)
        {
            if ((newState == IceConnectionState.Connected) || (newState == IceConnectionState.Completed))
            {
                iceConnectedEvent2_.Set();
            }
        }

        private void WaitForSdpExchangeCompleted()
        {
            Assert.True(connectedEvent1_.Wait(TimeSpan.FromSeconds(60.0)));
            Assert.True(connectedEvent2_.Wait(TimeSpan.FromSeconds(60.0)));
            connectedEvent1_.Reset();
            connectedEvent2_.Reset();
        }

        private unsafe void CustomI420AFrameCallback(in FrameRequest request)
        {
            var data = stackalloc byte[32 * 16 + 16 * 8 * 2];
            int k = 0;
            // Y plane (full resolution)
            for (int j = 0; j < 16; ++j)
            {
                for (int i = 0; i < 32; ++i)
                {
                    data[k++] = 0x7F;
                }
            }
            // U plane (halved chroma in both directions)
            for (int j = 0; j < 8; ++j)
            {
                for (int i = 0; i < 16; ++i)
                {
                    data[k++] = 0x30;
                }
            }
            // V plane (halved chroma in both directions)
            for (int j = 0; j < 8; ++j)
            {
                for (int i = 0; i < 16; ++i)
                {
                    data[k++] = 0xB2;
                }
            }
            var dataY = new IntPtr(data);
            var frame = new I420AVideoFrame
            {
                dataY = dataY,
                dataU = dataY + (32 * 16),
                dataV = dataY + (32 * 16) + (16 * 8),
                dataA = IntPtr.Zero,
                strideY = 32,
                strideU = 16,
                strideV = 16,
                strideA = 0,
                width = 32,
                height = 16
            };
            request.CompleteRequest(frame);
        }

        private unsafe void CustomArgb32FrameCallback(in FrameRequest request)
        {
            var data = stackalloc uint[32 * 16];
            int k = 0;
            // Create 2x2 checker pattern with 4 different colors
            for (int j = 0; j < 8; ++j)
            {
                for (int i = 0; i < 16; ++i)
                {
                    data[k++] = 0xFF0000FF;
                }
                for (int i = 16; i < 32; ++i)
                {
                    data[k++] = 0xFF00FF00;
                }
            }
            for (int j = 8; j < 16; ++j)
            {
                for (int i = 0; i < 16; ++i)
                {
                    data[k++] = 0xFFFF0000;
                }
                for (int i = 16; i < 32; ++i)
                {
                    data[k++] = 0xFF00FFFF;
                }
            }
            var frame = new Argb32VideoFrame
            {
                data = new IntPtr(data),
                stride = 128,
                width = 32,
                height = 16
            };
            request.CompleteRequest(frame);
        }

#if !MRSW_EXCLUDE_DEVICE_TESTS

        [Test]
        public async Task BeforeConnect()
        {
            // Create video transceiver on #1
            var transceiver1 = pc1_.AddVideoTransceiver();
            Assert.NotNull(transceiver1);

            // Wait for local SDP re-negotiation event on #1.
            // This will not create an offer, since we're not connected yet.
            Assert.True(renegotiationEvent1_.Wait(TimeSpan.FromSeconds(60.0)));

            // Create local video track
            var settings = new LocalVideoTrackSettings();
            LocalVideoTrack track1 = await LocalVideoTrack.CreateFromDeviceAsync(settings);
            Assert.NotNull(track1);

            // Add local video track channel to #1
            renegotiationEvent1_.Reset();
            transceiver1.SetLocalTrack(track1);
            Assert.IsFalse(renegotiationEvent1_.IsSet); // renegotiation not needed
            Assert.AreEqual(pc1_, track1.PeerConnection);
            Assert.AreEqual(track1, transceiver1.LocalTrack);
            Assert.IsNull(transceiver1.RemoteTrack);
            Assert.IsTrue(pc1_.VideoTransceivers.Contains(transceiver1));
            Assert.IsTrue(pc1_.LocalVideoTracks.Contains(track1));
            Assert.AreEqual(0, pc1_.RemoteVideoTracks.Count);

            // Connect
            Assert.True(pc1_.CreateOffer());
            WaitForSdpExchangeCompleted();
            Assert.True(pc1_.IsConnected);
            Assert.True(pc2_.IsConnected);

            // Now remote peer #2 has a 1 remote track
            Assert.AreEqual(0, pc2_.LocalVideoTracks.Count);
            Assert.AreEqual(1, pc2_.RemoteVideoTracks.Count);

            // Remove the track from #1
            renegotiationEvent1_.Reset();
            transceiver1.LocalTrack = null;
            Assert.IsFalse(renegotiationEvent1_.IsSet); // renegotiation not needed
            Assert.IsNull(track1.PeerConnection);
            Assert.IsNull(track1.Transceiver);
            Assert.AreEqual(0, pc1_.LocalVideoTracks.Count);
            Assert.IsTrue(pc1_.VideoTransceivers.Contains(transceiver1)); // never removed
            Assert.IsNull(transceiver1.LocalTrack);
            Assert.IsNull(transceiver1.RemoteTrack);
            track1.Dispose();

            //// Wait for local SDP re-negotiation on #1
            //Assert.True(renegotiationEvent1_.Wait(TimeSpan.FromSeconds(60.0)));

            //// Confirm remote track was removed from #2 -- not fired in Unified Plan
            ////Assert.True(videoTrackRemovedEvent2_.Wait(TimeSpan.FromSeconds(60.0)));

            //// Wait until SDP renegotiation finished
            //WaitForSdpExchangeCompleted();

            // Remote peer #2 doesn't have any track anymore
            Assert.AreEqual(0, pc2_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc2_.RemoteVideoTracks.Count);
        }

        [Test]
        public async Task AfterConnect()
        {
            // Connect
            Assert.True(pc1_.CreateOffer());
            WaitForSdpExchangeCompleted();
            Assert.True(pc1_.IsConnected);
            Assert.True(pc2_.IsConnected);

            // No track yet
            Assert.AreEqual(0, pc1_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc1_.RemoteVideoTracks.Count);
            Assert.AreEqual(0, pc2_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc2_.RemoteVideoTracks.Count);

            // Create video transceiver on #1
            renegotiationEvent1_.Reset();
            var transceiver1 = pc1_.AddVideoTransceiver();
            Assert.NotNull(transceiver1);
            Assert.AreEqual(transceiver1.DesiredDirection, Transceiver.Direction.Inactive);
            Assert.AreEqual(transceiver1.NegotiatedDirection, Transceiver.Direction.Inactive);
            Assert.IsTrue(pc1_.VideoTransceivers.Contains(transceiver1));

            // Wait for local SDP re-negotiation on #1
            Assert.True(renegotiationEvent1_.Wait(TimeSpan.FromSeconds(60.0)));

            // Confirm (inactive) remote track was added on #2 due to transceiver being added
            Assert.True(videoTrackAddedEvent2_.Wait(TimeSpan.FromSeconds(60.0)));

            // Wait until SDP renegotiation finished
            WaitForSdpExchangeCompleted();

            // Now remote peer #2 has a 1 remote track (which is inactive)
            Assert.AreEqual(0, pc2_.LocalVideoTracks.Count);
            Assert.AreEqual(1, pc2_.RemoteVideoTracks.Count);

            // Create local track
            renegotiationEvent1_.Reset();
            var settings = new LocalVideoTrackSettings();
            LocalVideoTrack track1 = await LocalVideoTrack.CreateFromDeviceAsync(settings);
            Assert.NotNull(track1);
            Assert.IsNull(track1.PeerConnection);
            Assert.IsNull(track1.Transceiver);
            Assert.IsFalse(renegotiationEvent1_.IsSet); // renegotiation not needed

            // Add local video track to #1
            renegotiationEvent1_.Reset();
            transceiver1.SetLocalTrack(track1);
            Assert.IsFalse(renegotiationEvent1_.IsSet); // renegotiation not needed
            Assert.AreEqual(pc1_, track1.PeerConnection);
            Assert.AreEqual(track1, transceiver1.LocalTrack);
            Assert.IsNull(transceiver1.RemoteTrack);
            Assert.IsTrue(pc1_.VideoTransceivers.Contains(transceiver1));
            Assert.IsTrue(pc1_.LocalVideoTracks.Contains(track1));
            Assert.AreEqual(0, pc1_.RemoteVideoTracks.Count);
            Assert.AreEqual(transceiver1.DesiredDirection, Transceiver.Direction.SendOnly);
            Assert.AreEqual(transceiver1.NegotiatedDirection, Transceiver.Direction.Inactive);

            // Remove the track from #1
            renegotiationEvent1_.Reset();
            transceiver1.SetLocalTrack(null);
            Assert.IsFalse(renegotiationEvent1_.IsSet); // renegotiation not needed
            Assert.IsNull(track1.PeerConnection);
            Assert.IsNull(track1.Transceiver);
            Assert.AreEqual(0, pc1_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc1_.RemoteVideoTracks.Count);
            Assert.IsTrue(pc1_.VideoTransceivers.Contains(transceiver1)); // never removed
            Assert.IsNull(transceiver1.LocalTrack);
            Assert.IsNull(transceiver1.RemoteTrack);
            Assert.AreEqual(transceiver1.DesiredDirection, Transceiver.Direction.Inactive);
            Assert.AreEqual(transceiver1.NegotiatedDirection, Transceiver.Direction.Inactive);
            track1.Dispose();

            // Confirm remote track was removed from #2 -- not fired in Unified Plan
            //Assert.True(trackRemovedEvent2_.Wait(TimeSpan.FromSeconds(60.0)));

            // Wait until SDP renegotiation finished
            //WaitForSdpExchangeCompleted();

            // Remote peer #2 doesn't have any track anymore
            Assert.AreEqual(0, pc2_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc2_.RemoteVideoTracks.Count);
        }

#endif // !MRSW_EXCLUDE_DEVICE_TESTS


        [Test]
        public void SimpleExternalI420A()
        {
            // Connect
            Assert.True(pc1_.CreateOffer());
            WaitForSdpExchangeCompleted();
            Assert.True(pc1_.IsConnected);
            Assert.True(pc2_.IsConnected);

            // Create external I420A source
            var source1 = ExternalVideoTrackSource.CreateFromI420ACallback(CustomI420AFrameCallback);
            Assert.NotNull(source1);

            // Add video transceiver
            renegotiationEvent1_.Reset();
            var transceiver1 = pc1_.AddVideoTransceiver();
            Assert.NotNull(transceiver1);
            Assert.IsTrue(renegotiationEvent1_.IsSet);

            // Create external I420A track
            var track1 = LocalVideoTrack.CreateFromExternalSource("custom_i420a", source1);
            Assert.NotNull(track1);

            // Add track on transceiver
            renegotiationEvent1_.Reset();
            transceiver1.SetLocalTrack(track1);
            Assert.AreEqual(pc1_, track1.PeerConnection);
            Assert.IsTrue(pc1_.LocalVideoTracks.Contains(track1));
            Assert.IsFalse(renegotiationEvent1_.IsSet); // renegotiation not needed

            // Wait for local SDP re-negotiation on #1
            Assert.True(renegotiationEvent1_.Wait(TimeSpan.FromSeconds(60.0)));

            // Confirm remote track was added on #2
            Assert.True(videoTrackAddedEvent2_.Wait(TimeSpan.FromSeconds(60.0)));

            // Wait until SDP renegotiation finished
            WaitForSdpExchangeCompleted();

            // Remove the track from #1
            renegotiationEvent1_.Reset();
            transceiver1.LocalTrack = null;
            Assert.IsNull(track1.PeerConnection);

            // Dispose of the track and its source
            track1.Dispose();
            track1 = null;
            source1.Dispose();
            source1 = null;

            // On peer #1 the track was replaced on the transceiver, but the transceiver stays
            // on the peer connection, so no renegotiation is needed.
            Assert.IsFalse(renegotiationEvent1_.IsSet);

            // Confirm remote track was removed from #2 -- not fired in Unified Plan
            //Assert.True(videoTrackRemovedEvent2_.Wait(TimeSpan.FromSeconds(60.0)));

            // Wait until SDP renegotiation finished
            //WaitForSdpExchangeCompleted();

            // Remote peer #2 doesn't have any track anymore
            Assert.AreEqual(0, pc2_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc2_.RemoteVideoTracks.Count);
        }

        [Test]
        public void SimpleExternalArgb32()
        {
            // Connect
            Assert.True(pc1_.CreateOffer());
            WaitForSdpExchangeCompleted();
            Assert.True(pc1_.IsConnected);
            Assert.True(pc2_.IsConnected);

            // No track yet
            Assert.AreEqual(0, pc1_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc1_.RemoteVideoTracks.Count);
            Assert.AreEqual(0, pc2_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc2_.RemoteVideoTracks.Count);

            // Create external ARGB32 source
            var source1 = ExternalVideoTrackSource.CreateFromArgb32Callback(CustomArgb32FrameCallback);
            Assert.NotNull(source1);

            // Add video transceiver #1
            var transceiver1 = pc1_.AddVideoTransceiver();
            Assert.NotNull(transceiver1);
            Assert.IsNull(transceiver1.LocalTrack);
            Assert.IsNull(transceiver1.RemoteTrack);
            Assert.AreEqual(pc1_, transceiver1.PeerConnection);

            // Create external ARGB32 track
            var track1 = LocalVideoTrack.CreateFromExternalSource("custom_argb32", source1);
            Assert.NotNull(track1);
            Assert.AreEqual(source1, track1.Source);
            Assert.IsNull(track1.PeerConnection);
            Assert.IsNull(track1.Transceiver);
            Assert.IsFalse(pc1_.LocalVideoTracks.Contains(track1));

            // Add track to transceiver
            transceiver1.SetLocalTrack(track1);
            Assert.AreEqual(pc1_, track1.PeerConnection);
            Assert.IsTrue(pc1_.LocalVideoTracks.Contains(track1));

            // Wait for local SDP re-negotiation on #1
            Assert.True(renegotiationEvent1_.Wait(TimeSpan.FromSeconds(60.0)));

            // Confirm remote track was added on #2
            Assert.True(videoTrackAddedEvent2_.Wait(TimeSpan.FromSeconds(60.0)));

            // Wait until SDP renegotiation finished
            WaitForSdpExchangeCompleted();

            // Remove the track from #1
            renegotiationEvent1_.Reset();
            transceiver1.LocalTrack = null;
            Assert.IsNull(track1.PeerConnection);
            Assert.IsNull(track1.Transceiver);
            Assert.IsFalse(pc1_.LocalVideoTracks.Contains(track1));

            // Dispose of the track and its source
            track1.Dispose();
            track1 = null;
            source1.Dispose();
            source1 = null;

            // Wait for local SDP re-negotiation on #1
            Assert.True(renegotiationEvent1_.Wait(TimeSpan.FromSeconds(60.0)));

            // Confirm remote track was removed from #2 -- not fired in Unified Plan
            //Assert.True(videoTrackRemovedEvent2_.Wait(TimeSpan.FromSeconds(60.0)));

            // Wait until SDP renegotiation finished
            WaitForSdpExchangeCompleted();

            // Remote peer #2 doesn't have any track anymore
            Assert.AreEqual(0, pc2_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc2_.RemoteVideoTracks.Count);
        }

        [Test]
        public void MultiExternalI420A()
        {
            // Connect
            Assert.True(pc1_.CreateOffer());
            WaitForSdpExchangeCompleted();
            Assert.True(pc1_.IsConnected);
            Assert.True(pc2_.IsConnected);

            // No track yet
            Assert.AreEqual(0, pc1_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc1_.RemoteVideoTracks.Count);
            Assert.AreEqual(0, pc2_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc2_.RemoteVideoTracks.Count);

            // Create external I420A source
            var source1 = ExternalVideoTrackSource.CreateFromI420ACallback(CustomI420AFrameCallback);
            Assert.NotNull(source1);
            Assert.AreEqual(0, source1.Tracks.Count);

            // Batch track changes
            suspendOffer1_ = true;

            // Add external I420A tracks
            const int kNumTracks = 5;
            var transceivers = new VideoTransceiver[kNumTracks];
            var tracks = new LocalVideoTrack[kNumTracks];
            for (int i = 0; i < kNumTracks; ++i)
            {
                transceivers[i] = pc1_.AddVideoTransceiver();
                Assert.NotNull(transceivers[i]);

                tracks[i] = LocalVideoTrack.CreateFromExternalSource("custom_i420a", source1);
                Assert.NotNull(tracks[i]);

                transceivers[i].LocalTrack = tracks[i];
                Assert.AreEqual(pc1_, tracks[i].PeerConnection);
                Assert.IsTrue(pc1_.LocalVideoTracks.Contains(tracks[i]));
                Assert.IsTrue(source1.Tracks.Contains(tracks[i]));
            }
            Assert.AreEqual(kNumTracks, source1.Tracks.Count);
            Assert.AreEqual(kNumTracks, pc1_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc1_.RemoteVideoTracks.Count);

            // Wait for local SDP re-negotiation on #1
            Assert.True(renegotiationEvent1_.Wait(TimeSpan.FromSeconds(60.0)));

            // Batch track changes
            suspendOffer1_ = false;
            pc1_.CreateOffer();

            // Confirm remote track was added on #2
            Assert.True(videoTrackAddedEvent2_.Wait(TimeSpan.FromSeconds(60.0)));

            // Wait until SDP renegotiation finished
            WaitForSdpExchangeCompleted();
            Assert.AreEqual(0, pc2_.LocalVideoTracks.Count);
            Assert.AreEqual(kNumTracks, pc2_.RemoteVideoTracks.Count);

            // Batch track changes
            suspendOffer1_ = true;

            // Remove the track from #1
            renegotiationEvent1_.Reset();
            for (int i = 0; i < kNumTracks; ++i)
            {
                transceivers[i].LocalTrack = null;
                Assert.IsNull(tracks[i].PeerConnection);
                Assert.IsFalse(pc1_.LocalVideoTracks.Contains(tracks[i]));
                Assert.IsFalse(source1.Tracks.Contains(tracks[i]));
                tracks[i].Dispose();
                tracks[i] = null;
            }
            Assert.AreEqual(0, pc1_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc1_.RemoteVideoTracks.Count);
            Assert.AreEqual(0, source1.Tracks.Count);

            // Dispose of source
            source1.Dispose();
            source1 = null;

            // Batch track changes
            suspendOffer1_ = false;
            pc1_.CreateOffer();

            // Wait for local SDP re-negotiation on #1
            Assert.True(renegotiationEvent1_.Wait(TimeSpan.FromSeconds(60.0)));

            // Confirm remote track was removed from #2 -- not fired in Unified Plan
            //Assert.True(videoTrackRemovedEvent2_.Wait(TimeSpan.FromSeconds(60.0)));

            // Wait until SDP renegotiation finished
            WaitForSdpExchangeCompleted();

            // Remote peer #2 doesn't have any track anymore
            Assert.AreEqual(0, pc2_.LocalVideoTracks.Count);
            Assert.AreEqual(0, pc2_.RemoteVideoTracks.Count);
        }
    }
}

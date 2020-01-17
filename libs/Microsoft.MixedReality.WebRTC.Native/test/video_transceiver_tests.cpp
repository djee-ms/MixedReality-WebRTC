// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/external_video_track_source_interop.h"
#include "interop/interop_api.h"
#include "interop/local_video_track_interop.h"
#include "interop/remote_video_track_interop.h"
#include "interop/video_transceiver_interop.h"

#include "simple_interop.h"

namespace {

const mrsPeerConnectionInteropHandle kFakeInteropPeerConnectionHandle =
    (void*)0x1;

const mrsRemoteVideoTrackInteropHandle kFakeInteropRemoteVideoTrackHandle =
    (void*)0x2;

const mrsVideoTransceiverInteropHandle kFakeInteropVideoTransceiverHandle =
    (void*)0x3;

/// Fake interop callback always returning the same fake remote video track
/// interop handle, for tests which do not care about it.
mrsRemoteVideoTrackInteropHandle MRS_CALL FakeIterop_RemoteVideoTrackCreate(
    mrsPeerConnectionInteropHandle /*parent*/,
    const mrsRemoteVideoTrackConfig& /*config*/) noexcept {
  return kFakeInteropRemoteVideoTrackHandle;
}

// PeerConnectionVideoTrackAddedCallback
using VideoTrackAddedCallback =
    InteropCallback<mrsRemoteVideoTrackInteropHandle,
                    RemoteVideoTrackHandle,
                    mrsVideoTransceiverInteropHandle,
                    VideoTransceiverHandle>;

// PeerConnectionI420VideoFrameCallback
using I420VideoFrameCallback = InteropCallback<const I420AVideoFrame&>;

}  // namespace

TEST(VideoTransceiver, SetDirection) {
  LocalPeerPairRaii pair;

  // In order to allow creating interop wrappers from native code, register the
  // necessary interop callbacks.
  mrsPeerConnectionInteropCallbacks interop{};
  interop.remote_video_track_create_object = &FakeIterop_RemoteVideoTrackCreate;
  ASSERT_EQ(Result::kSuccess,
            mrsPeerConnectionRegisterInteropCallbacks(pair.pc1(), &interop));
  ASSERT_EQ(Result::kSuccess,
            mrsPeerConnectionRegisterInteropCallbacks(pair.pc2(), &interop));

  // Register event for renegotiation needed
  Event renegotiation_needed1_ev;
  InteropCallback renegotiation_needed1_cb = [&renegotiation_needed1_ev]() {
    renegotiation_needed1_ev.Set();
  };
  mrsPeerConnectionRegisterRenegotiationNeededCallback(
      pair.pc1(), CB(renegotiation_needed1_cb));
  Event renegotiation_needed2_ev;
  InteropCallback renegotiation_needed2_cb = [&renegotiation_needed2_ev]() {
    renegotiation_needed2_ev.Set();
  };
  mrsPeerConnectionRegisterRenegotiationNeededCallback(
      pair.pc2(), CB(renegotiation_needed2_cb));

  // Add a transceiver to the local peer (#1)
  VideoTransceiverHandle transceiver_handle1{};
  {
    VideoTransceiverInitConfig transceiver_config{};
    transceiver_config.name = "video_transceiver_1";
    transceiver_config.transceiver_interop_handle =
        kFakeInteropVideoTransceiverHandle;
    renegotiation_needed1_ev.Reset();
    ASSERT_EQ(Result::kSuccess,
              mrsPeerConnectionAddVideoTransceiver(
                  pair.pc1(), &transceiver_config, &transceiver_handle1));
    ASSERT_NE(nullptr, transceiver_handle1);
    ASSERT_TRUE(renegotiation_needed1_ev.IsSignaled());
    renegotiation_needed1_ev.Reset();
  }

  // Register event for transceiver state update
  Event state_updated1_ev_local;
  Event state_updated1_ev_remote;
  Event state_updated1_ev_setdir;
  mrsTransceiverDirection dir_desired1 = mrsTransceiverDirection::kInactive;
  mrsTransceiverDirection dir_negotiated1 = mrsTransceiverDirection::kInactive;
  InteropCallback<mrsTransceiverStateUpdatedReason, mrsTransceiverDirection,
                  mrsTransceiverDirection>
      state_updated1_cb = [&](mrsTransceiverStateUpdatedReason reason,
                              mrsTransceiverDirection negotiated,
                              mrsTransceiverDirection desired) {
        dir_negotiated1 = negotiated;
        dir_desired1 = desired;
        switch (reason) {
          case mrsTransceiverStateUpdatedReason::kLocalDesc:
            state_updated1_ev_local.Set();
            break;
          case mrsTransceiverStateUpdatedReason::kRemoteDesc:
            state_updated1_ev_remote.Set();
            break;
          case mrsTransceiverStateUpdatedReason::kSetDirection:
            state_updated1_ev_setdir.Set();
            break;
        }
      };
  mrsVideoTransceiverRegisterStateUpdatedCallback(transceiver_handle1,
                                                  CB(state_updated1_cb));

  // Check video transceiver #1 consistency
  {
    // Default values inchanged (callback was just registered)
    ASSERT_EQ(mrsTransceiverDirection::kInactive, dir_negotiated1);
    ASSERT_EQ(mrsTransceiverDirection::kInactive, dir_desired1);

    // Local video track is NULL
    LocalVideoTrackHandle track_handle_local{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetLocalTrack(
                                    transceiver_handle1, &track_handle_local));
    ASSERT_EQ(nullptr, track_handle_local);

    // Remote video track is NULL
    RemoteVideoTrackHandle track_handle_remote{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetRemoteTrack(
                                    transceiver_handle1, &track_handle_remote));
    ASSERT_EQ(nullptr, track_handle_remote);
  }

  // Connect #1 and #2
  pair.ConnectAndWait();

  // Because the state updated event handler is registered after the transceiver
  // is created, the state is stale, and applying the local description during
  // |CreateOffer()| will generate some event.
  ASSERT_TRUE(state_updated1_ev_local.WaitFor(10s));
  state_updated1_ev_local.Reset();

  // Wait for transceiver to be updated; this happens *after* connect,
  // during SetRemoteDescription().
  ASSERT_TRUE(state_updated1_ev_remote.WaitFor(10s));
  state_updated1_ev_remote.Reset();

  // Check video transceiver #1 consistency
  {
    // Desired state is Send+Receive, negotiated is Send only because the remote
    // peer refused to send (no track added for that).
    ASSERT_EQ(mrsTransceiverDirection::kSendOnly, dir_negotiated1);
    ASSERT_EQ(mrsTransceiverDirection::kSendRecv, dir_desired1);
  }

  // Set transceiver #1 direction to Receive
  ASSERT_EQ(Result::kSuccess,
            mrsVideoTransceiverSetDirection(
                transceiver_handle1, mrsTransceiverDirection::kRecvOnly));
  ASSERT_TRUE(state_updated1_ev_setdir.IsSignaled());
  state_updated1_ev_setdir.Reset();

  // Check video transceiver #1 consistency
  {
    // Desired state is Receive, negotiated is still Send+Receive
    ASSERT_EQ(mrsTransceiverDirection::kSendOnly,
              dir_negotiated1);  // no change
    ASSERT_EQ(mrsTransceiverDirection::kRecvOnly, dir_desired1);
  }

  // Renegotiate
  pair.ConnectAndWait();

  // Wait for transceiver to be updated; this happens *after* connect, during
  // SetRemoteDescription()
  // Note: here the local description doesn't generate a state updated event
  // because the local state was set with SetDirection() so is already correct.
  // When the peer is creating the offer (#1), the desired direction is exactly
  // the one advertized in the local description.
  ASSERT_FALSE(state_updated1_ev_local.IsSignaled());
  ASSERT_TRUE(state_updated1_ev_remote.WaitFor(10s));
  state_updated1_ev_remote.Reset();

  // Check video transceiver #1 consistency
  {
    // Desired state is Receive, negotiated is Inactive because remote peer
    // refused to send (no track added for that).
    ASSERT_EQ(mrsTransceiverDirection::kInactive, dir_negotiated1);
    ASSERT_EQ(mrsTransceiverDirection::kRecvOnly, dir_desired1);
  }

  // Clean-up
  mrsVideoTransceiverRemoveRef(transceiver_handle1);
}


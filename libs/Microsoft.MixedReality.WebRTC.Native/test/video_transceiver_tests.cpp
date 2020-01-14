// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/external_video_track_source_interop.h"
#include "interop/interop_api.h"
#include "interop/local_video_track_interop.h"
#include "interop/remote_video_track_interop.h"
#include "interop/video_transceiver_interop.h"

#include "simple_interop.h"

#if !defined(MRSW_EXCLUDE_DEVICE_TESTS)

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

TEST(VideoTransceiver, Simple) {
  LibraryInitRaii lib;
  LocalPeerPairRaii pair;

  // In order to allow creating interop wrappers from native code, register the
  // necessary interop callbacks.
  mrsPeerConnectionInteropCallbacks interop{};
  interop.remote_video_track_create_object = &FakeIterop_RemoteVideoTrackCreate;
  ASSERT_EQ(Result::kSuccess,
            mrsPeerConnectionRegisterInteropCallbacks(pair.pc2(), &interop));

  // Grab the handle of the remote track from the remote peer (#2) via the
  // VideoTrackAdded callback.
  RemoteVideoTrackHandle track_handle2{};
  VideoTransceiverHandle transceiver_handle2{};
  Event track_added2_ev;
  VideoTrackAddedCallback track_added2_cb =
      [&track_handle2, &transceiver_handle2, &track_added2_ev](
          mrsRemoteVideoTrackInteropHandle /*track_interop_handle*/,
          RemoteVideoTrackHandle native_handle,
          mrsVideoTransceiverInteropHandle /*transceiver_interop_handle*/,
          VideoTransceiverHandle transceiver_handle) {
        track_handle2 = native_handle;
        transceiver_handle2 = transceiver_handle;
        track_added2_ev.Set();
      };
  mrsPeerConnectionRegisterVideoTrackAddedCallback(pair.pc2(),
                                                   CB(track_added2_cb));

  // Add a transceiver to the local peer (#1)
  VideoTransceiverHandle transceiver_handle1{};
  VideoTransceiverInitConfig transceiver_config{};
  transceiver_config.name = "video_transceiver";
  transceiver_config.transceiver_interop_handle =
      kFakeInteropVideoTransceiverHandle;
  ASSERT_EQ(Result::kSuccess,
            mrsPeerConnectionAddVideoTransceiver(
                pair.pc1(), &transceiver_config, &transceiver_handle1));
  ASSERT_NE(nullptr, transceiver_handle1);

  // Check video transceiver #1 consistency
  {
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

  // Create a new local track object for #1, but do not add it yet
  LocalVideoTrackInitConfig track_config{};
  LocalVideoTrackHandle track_handle1{};
  ASSERT_EQ(Result::kSuccess,
            mrsLocalVideoTrackCreateFromDevice(
                &track_config, "local_video_track", &track_handle1));
  ASSERT_NE(nullptr, track_handle1);

  // Set the video track of the transceiver on #1, which will add it to the peer
  // connection and send to the remote peer once negotiated.
  ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverSetLocalTrack(
                                  transceiver_handle1, track_handle1));

  // Check video transceiver #1 consistency
  {
    // Local video track is track_handle1
    LocalVideoTrackHandle track_handle_local{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetLocalTrack(
                                    transceiver_handle1, &track_handle_local));
    ASSERT_EQ(track_handle1, track_handle_local);
    mrsLocalVideoTrackRemoveRef(track_handle_local);

    // Remote video track is NULL
    RemoteVideoTrackHandle track_handle_remote{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetRemoteTrack(
                                    transceiver_handle1, &track_handle_remote));
    ASSERT_EQ(nullptr, track_handle_remote);
  }

  // Wait for remote track to be added on #2
  ASSERT_TRUE(track_added2_ev.WaitFor(5s));
  ASSERT_NE(nullptr, track_handle2);
  ASSERT_NE(nullptr, transceiver_handle2);

  // Register a frame callback for the remote video of #2
  uint32_t frame_count = 0;
  I420VideoFrameCallback i420cb = [&frame_count](const I420AVideoFrame& frame) {
    ASSERT_NE(nullptr, frame.ydata_);
    ASSERT_NE(nullptr, frame.udata_);
    ASSERT_NE(nullptr, frame.vdata_);
    ASSERT_LT(0u, frame.width_);
    ASSERT_LT(0u, frame.height_);
    ++frame_count;
  };
  mrsRemoteVideoTrackRegisterI420AFrameCallback(track_handle2, CB(i420cb));

  Event ev;
  ev.WaitFor(5s);
  ASSERT_LT(50u, frame_count) << "Expected at least 10 FPS";

  mrsRemoteVideoTrackRegisterI420AFrameCallback(track_handle2, nullptr,
                                                nullptr);
  mrsRemoteVideoTrackRemoveRef(track_handle2);
  mrsVideoTransceiverRemoveRef(transceiver_handle2);
  mrsLocalVideoTrackRemoveRef(track_handle1);
  mrsVideoTransceiverRemoveRef(transceiver_handle1);
}

#endif  // MRSW_EXCLUDE_DEVICE_TESTS

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

/// Fake interop callback always returning the same fake remote video track
/// interop handle, for tests which do not care about it.
mrsRemoteVideoTrackInteropHandle MRS_CALL FakeIterop_RemoteVideoTrackCreate(
    mrsPeerConnectionInteropHandle /*parent*/,
    const mrsRemoteVideoTrackConfig& /*config*/) noexcept {
  return kFakeInteropRemoteVideoTrackHandle;
}

// PeerConnectionVideoTrackAddedCallback
using VideoTrackAddedCallback =
    InteropCallback<mrsRemoteVideoTrackInteropHandle, RemoteVideoTrackHandle>;

// PeerConnectionI420VideoFrameCallback
using I420VideoFrameCallback = InteropCallback<const I420AVideoFrame&>;

}  // namespace

TEST(VideoTransceiver, Simple) {
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
  Event track_added2_ev;
  VideoTrackAddedCallback track_added2_cb =
      [&track_handle2, &track_added2_ev](
          mrsRemoteVideoTrackInteropHandle /*interop_handle*/,
          RemoteVideoTrackHandle native_handle) {
        track_handle2 = native_handle;
        track_added2_ev.Set();
      };
  mrsPeerConnectionRegisterVideoTrackAddedCallback(pair.pc2(),
                                                   CB(track_added2_cb));

  // Add a transceiver to the local peer (#1)
  VideoTransceiverHandle transceiver_handle1{};
  VideoTransceiverConfiguration config{};
  ASSERT_EQ(Result::kSuccess, mrsPeerConnectionAddVideoTransceiver(
                                  pair.pc1(), "video_transceiver", &config,
                                  &transceiver_handle1));
  ASSERT_NE(nullptr, transceiver_handle1);

  // Check video transceiver consistency
  {
    LocalVideoTrackHandle track_handle_local{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetLocalTrack(
                                    transceiver_handle1, &track_handle_local));
    ASSERT_EQ(nullptr, track_handle_local);

    RemoteVideoTrackHandle track_handle_remote{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetRemoteTrack(
                                    transceiver_handle1, &track_handle_remote));
    ASSERT_EQ(nullptr, track_handle_remote);
  }

  // Connect #1 and #2
  pair.ConnectAndWait();

  // Create a new local track object for #1, but do not add it yet
  VideoDeviceConfiguration config{};
  LocalVideoTrackHandle track_handle1{};
  ASSERT_EQ(
      Result::kSuccess,
      mrsLocalVideoTrackCreate("local_video_track", &config, &track_handle1));
  ASSERT_NE(nullptr, track_handle1);

  // Set the video track of the transceiver on #1
  ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverSetLocalTrack(
                                  transceiver_handle1, track_handle1));





  // Wait for remote track to be added on #2
  ASSERT_TRUE(track_added2_ev.WaitFor(5s));
  ASSERT_NE(nullptr, track_handle2);

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
  mrsLocalVideoTrackRemoveRef(track_handle);
}

#endif  // MRSW_EXCLUDE_DEVICE_TESTS

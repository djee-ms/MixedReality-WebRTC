// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "data_channel.h"
#include "interop/external_video_track_source_interop.h"
#include "interop/interop_api.h"
#include "interop/local_video_track_interop.h"
#include "interop/remote_video_track_interop.h"
#include "interop/video_transceiver_interop.h"

#include "libyuv.h"

#if !defined(MRSW_EXCLUDE_DEVICE_TESTS)

namespace {

uint32_t FrameBuffer[256];

void FillSquareArgb32(uint32_t* buffer,
                      int x,
                      int y,
                      int w,
                      int h,
                      int stride,
                      uint32_t color) {
  assert(stride % 4 == 0);
  char* row = ((char*)buffer + (y * stride));
  for (int j = 0; j < h; ++j) {
    uint32_t* ptr = (uint32_t*)row + x;
    for (int i = 0; i < w; ++i) {
      *ptr++ = color;
    }
    row += stride;
  }
}

constexpr uint32_t kRed = 0xFF2250F2u;
constexpr uint32_t kGreen = 0xFF00BA7Fu;
constexpr uint32_t kBlue = 0xFFEFA400u;
constexpr uint32_t kYellow = 0xFF00B9FFu;

/// Generate a 16px by 16px test frame.
mrsResult MRS_CALL
GenerateQuadTestFrame(void* /*user_data*/,
                      ExternalVideoTrackSourceHandle source_handle,
                      uint32_t request_id,
                      int64_t timestamp_ms) {
  memset(FrameBuffer, 0, 256 * 4);
  FillSquareArgb32(FrameBuffer, 0, 0, 8, 8, 64, kRed);
  FillSquareArgb32(FrameBuffer, 8, 0, 8, 8, 64, kGreen);
  FillSquareArgb32(FrameBuffer, 0, 8, 8, 8, 64, kBlue);
  FillSquareArgb32(FrameBuffer, 8, 8, 8, 8, 64, kYellow);
  mrsArgb32VideoFrame frame_view{};
  frame_view.width_ = 16;
  frame_view.height_ = 16;
  frame_view.argb32_data_ = FrameBuffer;
  frame_view.stride_ = 16 * 4;
  return mrsExternalVideoTrackSourceCompleteArgb32FrameRequest(
      source_handle, request_id, timestamp_ms, &frame_view);
}

inline double ArgbColorError(uint32_t ref, uint32_t val) {
  return ((double)(ref & 0xFFu) - (double)(val & 0xFFu)) +
         ((double)((ref & 0xFF00u) >> 8u) - (double)((val & 0xFF00u) >> 8u)) +
         ((double)((ref & 0xFF0000u) >> 16u) -
          (double)((val & 0xFF0000u) >> 16u)) +
         ((double)((ref & 0xFF000000u) >> 24u) -
          (double)((val & 0xFF000000u) >> 24u));
}

void ValidateQuadTestFrame(const void* data,
                           const int stride,
                           const int frame_width,
                           const int frame_height) {
  ASSERT_NE(nullptr, data);
  ASSERT_EQ(16, frame_width);
  ASSERT_EQ(16, frame_height);
  double err = 0.0;
  const uint8_t* row = (const uint8_t*)data;
  for (int j = 0; j < 8; ++j) {
    const uint32_t* argb = (const uint32_t*)row;
    // Red
    for (int i = 0; i < 8; ++i) {
      err += ArgbColorError(kRed, *argb++);
    }
    // Green
    for (int i = 0; i < 8; ++i) {
      err += ArgbColorError(kGreen, *argb++);
    }
    row += stride;
  }
  for (int j = 0; j < 8; ++j) {
    const uint32_t* argb = (const uint32_t*)row;
    // Blue
    for (int i = 0; i < 8; ++i) {
      err += ArgbColorError(kBlue, *argb++);
    }
    // Yellow
    for (int i = 0; i < 8; ++i) {
      err += ArgbColorError(kYellow, *argb++);
    }
    row += stride;
  }
  ASSERT_LE(std::fabs(err), 768.0);  // +/-1 per component over 256 pixels
}

// PeerConnectionVideoTrackAddedCallback
using VideoTrackAddedCallback =
    InteropCallback<mrsRemoteVideoTrackInteropHandle,
                    RemoteVideoTrackHandle,
                    mrsVideoTransceiverInteropHandle,
                    VideoTransceiverHandle>;

// PeerConnectionArgb32VideoFrameCallback
using Argb32VideoFrameCallback = InteropCallback<const mrsArgb32VideoFrame&>;

}  // namespace

TEST(ExternalVideoTrackSource, Simple) {
  LocalPeerPairRaii pair;

  // Grab the handle of the remote track from the remote peer (#2) via the
  // VideoTrackAdded callback.
  RemoteVideoTrackHandle track_handle2{};
  VideoTransceiverHandle transceiver_handle2{};
  Event track_added2_ev;
  VideoTrackAddedCallback track_added2_cb =
      [&track_handle2, &transceiver_handle2, &track_added2_ev](
          mrsRemoteVideoTrackInteropHandle /*interop_handle*/,
          RemoteVideoTrackHandle track_handle,
          mrsVideoTransceiverInteropHandle /*interop_handle*/,
          VideoTransceiverHandle transceiver_handle) {
        track_handle2 = track_handle;
        transceiver_handle2 = transceiver_handle;
        track_added2_ev.Set();
      };
  mrsPeerConnectionRegisterVideoTrackAddedCallback(pair.pc2(),
                                                   CB(track_added2_cb));

  // Create the external source for the local video track of the local peer (#1)
  ExternalVideoTrackSourceHandle source_handle1 = nullptr;
  ASSERT_EQ(mrsResult::kSuccess,
            mrsExternalVideoTrackSourceCreateFromArgb32Callback(
                &GenerateQuadTestFrame, nullptr, &source_handle1));
  ASSERT_NE(nullptr, source_handle1);
  mrsExternalVideoTrackSourceFinishCreation(source_handle1);

  // Create the local track itself for #1
  LocalVideoTrackHandle track_handle1{};
  {
    LocalVideoTrackFromExternalSourceInitConfig config{};
    ASSERT_EQ(mrsResult::kSuccess,
              mrsLocalVideoTrackCreateFromExternalSource(
                  source_handle1, &config, "gen_track", &track_handle1));
    ASSERT_NE(nullptr, track_handle1);
    ASSERT_NE(mrsBool::kFalse, mrsLocalVideoTrackIsEnabled(track_handle1));
  }

  // Create the video transceiver #1
  VideoTransceiverHandle transceiver_handle1{};
  {
    VideoTransceiverInitConfig config{};
    config.name = "transceiver_1";
    ASSERT_EQ(mrsResult::kSuccess,
              mrsPeerConnectionAddVideoTransceiver(pair.pc1(), &config,
                                                   &transceiver_handle1));
    ASSERT_NE(nullptr, transceiver_handle1);
  }

  // Check video transceiver #1 consistency
  {
    // Local track is NULL
    LocalVideoTrackHandle track_handle_local{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetLocalTrack(
                                    transceiver_handle1, &track_handle_local));
    ASSERT_EQ(nullptr, track_handle_local);

    // Remote track is NULL
    RemoteVideoTrackHandle track_handle_remote{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetRemoteTrack(
                                    transceiver_handle1, &track_handle_remote));
    ASSERT_EQ(nullptr, track_handle_remote);
  }

  // Add the track #1 to the transceiver #1
  ASSERT_EQ(mrsResult::kSuccess, mrsVideoTransceiverSetLocalTrack(
                                     transceiver_handle1, track_handle1));

  // Check video transceiver #1 consistency
  {
    // Local track is track_handle1
    LocalVideoTrackHandle track_handle_local{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetLocalTrack(
                                    transceiver_handle1, &track_handle_local));
    ASSERT_EQ(track_handle1, track_handle_local);
    mrsLocalVideoTrackRemoveRef(track_handle_local);

    // Remote track is NULL
    RemoteVideoTrackHandle track_handle_remote{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetRemoteTrack(
                                    transceiver_handle1, &track_handle_remote));
    ASSERT_EQ(nullptr, track_handle_remote);
  }

  // Connect #1 and #2
  pair.ConnectAndWait();

  // Wait for remote track to be added on #2
  ASSERT_TRUE(track_added2_ev.WaitFor(5s));
  ASSERT_NE(nullptr, track_handle2);
  ASSERT_NE(nullptr, transceiver_handle2);

  // Check video transceiver #2 consistency
  {
    // Local track is NULL
    LocalVideoTrackHandle track_handle_local{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetLocalTrack(
                                    transceiver_handle2, &track_handle_local));
    ASSERT_EQ(nullptr, track_handle_local);

    // Remote track is track_handle2
    RemoteVideoTrackHandle track_handle_remote{};
    ASSERT_EQ(Result::kSuccess, mrsVideoTransceiverGetRemoteTrack(
                                    transceiver_handle2, &track_handle_remote));
    ASSERT_EQ(track_handle2, track_handle_remote);
    mrsRemoteVideoTrackRemoveRef(track_handle_remote);
  }

  // Register a frame callback for the remote video of #2
  uint32_t frame_count = 0;
  Argb32VideoFrameCallback argb_cb =
      [&frame_count](const mrsArgb32VideoFrame& frame) {
        ASSERT_NE(nullptr, frame.argb32_data_);
        ASSERT_LT(0u, frame.width_);
        ASSERT_LT(0u, frame.height_);
        ValidateQuadTestFrame(frame.argb32_data_, frame.stride_, frame.width_,
                              frame.height_);
        ++frame_count;
      };
  mrsRemoteVideoTrackRegisterArgb32FrameCallback(track_handle2, CB(argb_cb));

  // Wait 5 seconds and check the frame callback is called
  Event ev;
  ev.WaitFor(5s);
  ASSERT_LT(50u, frame_count) << "Expected at least 10 FPS";

  // Clean-up
  mrsRemoteVideoTrackRegisterArgb32FrameCallback(track_handle2, nullptr,
                                                 nullptr);
  mrsRemoteVideoTrackRemoveRef(track_handle2);
  mrsVideoTransceiverRemoveRef(transceiver_handle2);
  mrsLocalVideoTrackRemoveRef(track_handle1);
  mrsVideoTransceiverRemoveRef(transceiver_handle1);
  mrsExternalVideoTrackSourceShutdown(source_handle1);
  mrsExternalVideoTrackSourceRemoveRef(source_handle1);
}

#endif  // MRSW_EXCLUDE_DEVICE_TESTS

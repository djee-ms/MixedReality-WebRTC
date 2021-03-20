// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "interop_api.h"

extern "C" {

/// Statistics about an external video track source.
struct mrsExternalVideoTrackSourceStats {
  /// Number of frames produced since the source started capture.
  uint64_t num_frames_produced_{0};
  /// Number of frames skipped since the source started capture.
  uint64_t num_frames_skipped_{0};
  /// Average framerate of produced frames over the past second.
  float avg_framerate_{0};
};

/// Create a custom video track source external to the implementation. This
/// allows feeding into WebRTC frames from any source, including generated or
/// synthetic frames, for example for testing. The frame is provided from a
/// callback as an I420-encoded buffer. This returns a handle to a newly
/// allocated object, which must be released once not used anymore with
/// |mrsRefCountedObjectRemoveRef()|.
MRS_API mrsResult MRS_CALL mrsExternalVideoTrackSourceCreateFromI420ACallback(
    mrsRequestExternalI420AVideoFrameCallback callback,
    void* user_data,
    mrsExternalVideoTrackSourceHandle* source_handle_out) noexcept;

/// Create a custom video track source external to the implementation. This
/// allows feeding into WebRTC frames from any source, including generated or
/// synthetic frames, for example for testing. The frame is provided from a
/// callback as an ARGB32-encoded buffer. This returns a handle to a newly
/// allocated object, which must be released once not used anymore with
/// |mrsRefCountedObjectRemoveRef()|.
MRS_API mrsResult MRS_CALL mrsExternalVideoTrackSourceCreateFromArgb32Callback(
    mrsRequestExternalArgb32VideoFrameCallback callback,
    void* user_data,
    mrsExternalVideoTrackSourceHandle* source_handle_out) noexcept;

/// Set the target framerate of the video track source, which determines the
/// frequency at which the callback is invoked to get new frames. The default
/// frequency is 30 FPS. Valid values range from >0 FPS to 240 FPS.
MRS_API mrsResult MRS_CALL mrsExternalVideoTrackSourceSetFramerate(
    mrsExternalVideoTrackSourceHandle handle,
    float framerate) noexcept;

/// Get the target framerate of the video track source, which determines the
/// frequency at which the callback is invoked to get new frames. The default
/// frequency is 30 FPS.
MRS_API mrsResult MRS_CALL mrsExternalVideoTrackSourceGetFramerate(
    mrsExternalVideoTrackSourceHandle handle,
    float* framerate) noexcept;

/// Callback from the wrapper layer indicating that the wrapper has finished
/// creation, and it is safe to start sending frame requests to it. This needs
/// to be called after |mrsExternalVideoTrackSourceCreateFromI420ACallback()| or
/// |mrsExternalVideoTrackSourceCreateFromArgb32Callback()| to finish the
/// creation of the video track source and allow it to start capturing.
MRS_API void MRS_CALL mrsExternalVideoTrackSourceFinishCreation(
    mrsExternalVideoTrackSourceHandle source_handle) noexcept;

/// Complete a video frame request with a provided I420A video frame.
MRS_API mrsResult MRS_CALL mrsExternalVideoTrackSourceCompleteI420AFrameRequest(
    mrsExternalVideoTrackSourceHandle handle,
    uint32_t request_id,
    int64_t timestamp_ms,
    const mrsI420AVideoFrame* frame_view) noexcept;

/// Complete a video frame request with a provided ARGB32 video frame.
MRS_API mrsResult MRS_CALL
mrsExternalVideoTrackSourceCompleteArgb32FrameRequest(
    mrsExternalVideoTrackSourceHandle handle,
    uint32_t request_id,
    int64_t timestamp_ms,
    const mrsArgb32VideoFrame* frame_view) noexcept;

/// Get some statistics about the external video track source.
MRS_API mrsResult MRS_CALL mrsExternalVideoTrackSourceGetStats(
    mrsExternalVideoTrackSourceHandle handle,
    mrsExternalVideoTrackSourceStats* stats) noexcept;

/// Irreversibly stop the video source frame production and shutdown the video
/// source.
MRS_API void MRS_CALL mrsExternalVideoTrackSourceShutdown(
    mrsExternalVideoTrackSourceHandle handle) noexcept;

}  // extern "C"

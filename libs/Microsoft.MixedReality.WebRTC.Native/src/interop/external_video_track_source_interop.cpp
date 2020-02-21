// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// This is a precompiled header, it must be on its own, followed by a blank
// line, to prevent clang-format from reordering it with other headers.
#include "pch.h"

#include "callback.h"
#include "media/external_video_track_source.h"
#include "external_video_track_source_interop.h"

using namespace Microsoft::MixedReality::WebRTC;

void MRS_CALL mrsExternalVideoTrackSourceAddRef(
    ExternalVideoTrackSourceHandle handle) noexcept {
  if (auto track = static_cast<ExternalVideoTrackSource*>(handle)) {
    track->AddRef();
  } else {
    RTC_LOG(LS_WARNING)
        << "Trying to add reference to NULL ExternalVideoTrackSource object.";
  }
}

void MRS_CALL mrsExternalVideoTrackSourceRemoveRef(
    ExternalVideoTrackSourceHandle handle) noexcept {
  if (auto track = static_cast<ExternalVideoTrackSource*>(handle)) {
    track->RemoveRef();
  } else {
    RTC_LOG(LS_WARNING) << "Trying to remove reference from NULL "
                           "ExternalVideoTrackSource object.";
  }
}

mrsResult MRS_CALL mrsExternalVideoTrackSourceCreateFromI420ACallback(
    mrsRequestExternalI420AVideoFrameCallback callback,
    void* user_data,
    ExternalVideoTrackSourceHandle* source_handle_out) noexcept {
  if (!source_handle_out) {
    return Result::kInvalidParameter;
  }
  *source_handle_out = nullptr;
  RefPtr<ExternalVideoTrackSource> track_source =
      detail::ExternalVideoTrackSourceCreateFromI420A(callback, user_data);
  if (!track_source) {
    return Result::kUnknownError;
  }
  *source_handle_out = track_source.release();
  return Result::kSuccess;
}

mrsResult MRS_CALL mrsExternalVideoTrackSourceCreateFromArgb32Callback(
    mrsRequestExternalArgb32VideoFrameCallback callback,
    void* user_data,
    ExternalVideoTrackSourceHandle* source_handle_out) noexcept {
  if (!source_handle_out) {
    return Result::kInvalidParameter;
  }
  *source_handle_out = nullptr;
  RefPtr<ExternalVideoTrackSource> track_source =
      detail::ExternalVideoTrackSourceCreateFromArgb32(callback, user_data);
  if (!track_source) {
    return Result::kUnknownError;
  }
  *source_handle_out = track_source.release();
  return Result::kSuccess;
}

void MRS_CALL mrsExternalVideoTrackSourceFinishCreation(
    ExternalVideoTrackSourceHandle source_handle) noexcept {
  if (auto source = static_cast<ExternalVideoTrackSource*>(source_handle)) {
    source->FinishCreation();
  }
}

mrsResult MRS_CALL mrsExternalVideoTrackSourceCompleteI420AFrameRequest(
    ExternalVideoTrackSourceHandle handle,
    uint32_t request_id,
    int64_t timestamp_ms,
    const mrsI420AVideoFrame* frame_view) noexcept {
  if (!frame_view) {
    return Result::kInvalidParameter;
  }
  if (auto track = static_cast<ExternalVideoTrackSource*>(handle)) {
    return track->CompleteRequest(request_id, timestamp_ms, *frame_view);
  }
  return mrsResult::kInvalidNativeHandle;
}

mrsResult MRS_CALL mrsExternalVideoTrackSourceCompleteArgb32FrameRequest(
    ExternalVideoTrackSourceHandle handle,
    uint32_t request_id,
    int64_t timestamp_ms,
    const mrsArgb32VideoFrame* frame_view) noexcept {
  if (!frame_view) {
    return Result::kInvalidParameter;
  }
  if (auto track = static_cast<ExternalVideoTrackSource*>(handle)) {
    return track->CompleteRequest(request_id, timestamp_ms, *frame_view);
  }
  return mrsResult::kInvalidNativeHandle;
}

void MRS_CALL mrsExternalVideoTrackSourceShutdown(
    ExternalVideoTrackSourceHandle handle) noexcept {
  if (auto track = static_cast<ExternalVideoTrackSource*>(handle)) {
    track->Shutdown();
  }
}

namespace {

/// Adapter for a an interop-based I420A custom video source.
struct I420AInteropVideoSource : I420AExternalVideoSource {
  using callback_type =
      RetCallback<mrsResult, ExternalVideoTrackSourceHandle, uint32_t, int64_t>;

  /// Interop callback to generate frames.
  callback_type callback_;

  /// External video track source to deliver the frames to.
  RefPtr<ExternalVideoTrackSource> track_source_;

  I420AInteropVideoSource(mrsRequestExternalI420AVideoFrameCallback callback,
                          void* user_data)
      : callback_({callback, user_data}) {}

  Result FrameRequested(I420AVideoFrameRequest& frame_request) override {
    assert(track_source_);
    return callback_(track_source_.get(), frame_request.request_id_,
                     frame_request.timestamp_ms_);
  }
};

/// Adapter for a an interop-based ARGB32 custom video source.
struct Argb32InteropVideoSource : Argb32ExternalVideoSource {
  using callback_type =
      RetCallback<mrsResult, ExternalVideoTrackSourceHandle, uint32_t, int64_t>;

  /// Interop callback to generate frames.
  callback_type callback_;

  /// External video track source to deliver the frames to.
  RefPtr<ExternalVideoTrackSource> track_source_;

  Argb32InteropVideoSource(mrsRequestExternalArgb32VideoFrameCallback callback,
                           void* user_data)
      : callback_({callback, user_data}) {}

  Result FrameRequested(Argb32VideoFrameRequest& frame_request) override {
    assert(track_source_);
    return callback_(track_source_.get(), frame_request.request_id_,
                     frame_request.timestamp_ms_);
  }
};

}  // namespace

namespace Microsoft::MixedReality::WebRTC::detail {

RefPtr<ExternalVideoTrackSource> ExternalVideoTrackSourceCreateFromI420A(
    mrsRequestExternalI420AVideoFrameCallback callback,
    void* user_data) {
  RefPtr<I420AInteropVideoSource> custom_source =
      new I420AInteropVideoSource(callback, user_data);
  if (!custom_source) {
    return {};
  }
  RefPtr<ExternalVideoTrackSource> track_source =
      ExternalVideoTrackSource::createFromI420A(custom_source);
  if (!track_source) {
    return {};
  }
  custom_source->track_source_ = track_source;
  return track_source;
}

RefPtr<ExternalVideoTrackSource> ExternalVideoTrackSourceCreateFromArgb32(
    mrsRequestExternalArgb32VideoFrameCallback callback,
    void* user_data) {
  RefPtr<Argb32InteropVideoSource> custom_source =
      new Argb32InteropVideoSource(callback, user_data);
  if (!custom_source) {
    return {};
  }
  RefPtr<ExternalVideoTrackSource> track_source =
      ExternalVideoTrackSource::createFromArgb32(custom_source);
  if (!track_source) {
    return {};
  }
  custom_source->track_source_ = track_source;
  return track_source;
}

}  // namespace Microsoft::MixedReality::WebRTC::detail

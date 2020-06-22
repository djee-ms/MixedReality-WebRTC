// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// This is a precompiled header, it must be on its own, followed by a blank
// line, to prevent clang-format from reordering it with other headers.
#include "pch.h"

#include "callback.h"
#include "external_audio_track_source_interop.h"
#include "interop/global_factory.h"
#include "media/external_audio_track_source.h"

using namespace Microsoft::MixedReality::WebRTC;

mrsResult MRS_CALL mrsExternalAudioTrackSourceCreateFromCallback(
    mrsRequestExternalAudioFrameCallback callback,
    void* user_data,
    mrsExternalAudioTrackSourceHandle* source_handle_out) noexcept {
  if (!source_handle_out) {
    return Result::kInvalidParameter;
  }
  *source_handle_out = nullptr;
  RefPtr<ExternalAudioTrackSource> track_source =
      detail::ExternalAudioTrackSourceCreate(GlobalFactory::InstancePtr(),
                                             callback, user_data);
  if (!track_source) {
    return Result::kUnknownError;
  }
  *source_handle_out = track_source.release();
  return Result::kSuccess;
}

void MRS_CALL mrsExternalAudioTrackSourceFinishCreation(
    mrsExternalAudioTrackSourceHandle source_handle) noexcept {
  if (auto source = static_cast<ExternalAudioTrackSource*>(source_handle)) {
    source->FinishCreation();
  }
}

mrsResult MRS_CALL mrsExternalAudioTrackSourceCompleteFrameRequest(
    mrsExternalAudioTrackSourceHandle handle,
    uint32_t request_id,
    int64_t timestamp_ms,
    const mrsAudioFrame* frame_view) noexcept {
  if (!frame_view) {
    return Result::kInvalidParameter;
  }
  if (auto track = static_cast<ExternalAudioTrackSource*>(handle)) {
    return track->CompleteRequest(request_id, timestamp_ms, *frame_view);
  }
  return mrsResult::kInvalidNativeHandle;
}

void MRS_CALL mrsExternalAudioTrackSourceShutdown(
    mrsExternalAudioTrackSourceHandle handle) noexcept {
  if (auto track = static_cast<ExternalAudioTrackSource*>(handle)) {
    track->Shutdown();
  }
}

namespace {

/// Adapter for a an interop-based custom audio source.
struct InteropAudioSource : ExternalAudioSource {
  using callback_type = RetCallback<mrsResult,
                                    mrsExternalAudioTrackSourceHandle,
                                    uint32_t,
                                    int64_t>;

  /// Interop callback to generate frames.
  callback_type callback_;

  /// External audio track source to deliver the frames to.
  /// Note that this is a "weak" pointer to avoid a circular reference to the
  /// audio track source owning it.
  ExternalAudioTrackSource* track_source_{};

  InteropAudioSource(mrsRequestExternalAudioFrameCallback callback,
                     void* user_data)
      : callback_({callback, user_data}) {}

  Result FrameRequested(AudioFrameRequest& frame_request) override {
    assert(track_source_);
    return callback_(track_source_, frame_request.request_id_,
                     frame_request.timestamp_ms_);
  }
};

}  // namespace

namespace Microsoft {
namespace MixedReality {
namespace WebRTC {
namespace detail {

RefPtr<ExternalAudioTrackSource> ExternalAudioTrackSourceCreate(
    RefPtr<GlobalFactory> global_factory,
    mrsRequestExternalAudioFrameCallback callback,
    void* user_data) {
  RefPtr<InteropAudioSource> custom_source =
      new InteropAudioSource(callback, user_data);
  if (!custom_source) {
    return {};
  }
  // Tracks need to be created from the worker thread
  rtc::Thread* const worker_thread = global_factory->GetWorkerThread();
  auto track_source = worker_thread->Invoke<RefPtr<ExternalAudioTrackSource>>(
      RTC_FROM_HERE, rtc::Bind(&ExternalAudioTrackSource::create,
                               std::move(global_factory), custom_source));
  if (!track_source) {
    return {};
  }
  custom_source->track_source_ = track_source.get();
  return track_source;
}

}  // namespace detail
}  // namespace WebRTC
}  // namespace MixedReality
}  // namespace Microsoft

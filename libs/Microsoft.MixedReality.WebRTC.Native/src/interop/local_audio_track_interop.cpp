// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// This is a precompiled header, it must be on its own, followed by a blank
// line, to prevent clang-format from reordering it with other headers.
#include "pch.h"

#include "interop/local_audio_track_interop.h"
#include "media/local_audio_track.h"

using namespace Microsoft::MixedReality::WebRTC;

void MRS_CALL mrsLocalAudioTrackAddRef(LocalAudioTrackHandle handle) noexcept {
  if (auto track = static_cast<LocalAudioTrack*>(handle)) {
    track->AddRef();
  } else {
    RTC_LOG(LS_WARNING)
        << "Trying to add reference to NULL LocalAudioTrack object.";
  }
}

void MRS_CALL
mrsLocalAudioTrackRemoveRef(LocalAudioTrackHandle handle) noexcept {
  if (auto track = static_cast<LocalAudioTrack*>(handle)) {
    track->RemoveRef();
  } else {
    RTC_LOG(LS_WARNING) << "Trying to remove reference from NULL "
                           "LocalAudioTrack object.";
  }
}

void MRS_CALL
mrsLocalAudioTrackRegisterFrameCallback(LocalAudioTrackHandle trackHandle,
                                        mrsAudioFrameCallback callback,
                                        void* user_data) noexcept {
  if (auto track = static_cast<LocalAudioTrack*>(trackHandle)) {
    track->SetCallback(AudioFrameReadyCallback{callback, user_data});
  }
}

mrsResult MRS_CALL
mrsLocalAudioTrackSetEnabled(LocalAudioTrackHandle track_handle,
                             mrsBool enabled) noexcept {
  auto track = static_cast<LocalAudioTrack*>(track_handle);
  if (!track) {
    return Result::kInvalidParameter;
  }
  track->SetEnabled(enabled != mrsBool::kFalse);
  return Result::kSuccess;
}

mrsBool MRS_CALL
mrsLocalAudioTrackIsEnabled(LocalAudioTrackHandle track_handle) noexcept {
  auto track = static_cast<LocalAudioTrack*>(track_handle);
  if (!track) {
    return mrsBool::kFalse;
  }
  return (track->IsEnabled() ? mrsBool::kTrue : mrsBool::kFalse);
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// This is a precompiled header, it must be on its own, followed by a blank
// line, to prevent clang-format from reordering it with other headers.
#include "pch.h"

#include "media/remote_audio_track.h"
#include "remote_audio_track_interop.h"

using namespace Microsoft::MixedReality::WebRTC;

void MRS_CALL
mrsRemoteAudioTrackAddRef(mrsRemoteAudioTrackHandle handle) noexcept {
  if (auto track = static_cast<RemoteAudioTrack*>(handle)) {
    track->AddRef();
  } else {
    RTC_LOG(LS_WARNING)
        << "Trying to add reference to NULL RemoteAudioTrack object.";
  }
}

void MRS_CALL
mrsRemoteAudioTrackRemoveRef(mrsRemoteAudioTrackHandle handle) noexcept {
  if (auto track = static_cast<RemoteAudioTrack*>(handle)) {
    track->RemoveRef();
  } else {
    RTC_LOG(LS_WARNING) << "Trying to remove reference from NULL "
                           "RemoteAudioTrack object.";
  }
}

void MRS_CALL
mrsRemoteAudioTrackRegisterFrameCallback(mrsRemoteAudioTrackHandle trackHandle,
                                         mrsAudioFrameCallback callback,
                                         void* user_data) noexcept {
  if (auto track = static_cast<RemoteAudioTrack*>(trackHandle)) {
    track->SetCallback(AudioFrameReadyCallback{callback, user_data});
  }
}

mrsResult MRS_CALL
mrsRemoteAudioTrackSetEnabled(mrsRemoteAudioTrackHandle track_handle,
                              mrsBool enabled) noexcept {
  auto track = static_cast<RemoteAudioTrack*>(track_handle);
  if (!track) {
    return Result::kInvalidParameter;
  }
  track->SetEnabled(enabled != mrsBool::kFalse);
  return Result::kSuccess;
}

mrsBool MRS_CALL
mrsRemoteAudioTrackIsEnabled(mrsRemoteAudioTrackHandle track_handle) noexcept {
  auto track = static_cast<RemoteAudioTrack*>(track_handle);
  if (!track) {
    return mrsBool::kFalse;
  }
  return (track->IsEnabled() ? mrsBool::kTrue : mrsBool::kFalse);
}

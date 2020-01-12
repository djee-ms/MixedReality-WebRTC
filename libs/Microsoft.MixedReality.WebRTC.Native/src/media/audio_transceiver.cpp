// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "audio_transceiver.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

AudioTransceiver::AudioTransceiver(PeerConnection& owner) noexcept
    : Transceiver(MediaKind::kAudio, owner) {}

AudioTransceiver::AudioTransceiver(
    PeerConnection& owner,
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver,
    mrsAudioTransceiverInteropHandle interop_handle) noexcept
    : Transceiver(MediaKind::kAudio, owner, transceiver),
      interop_handle_(interop_handle) {}

AudioTransceiver::~AudioTransceiver() {}

Result AudioTransceiver::SetLocalTrack(
    RefPtr<LocalAudioTrack> local_track) noexcept {
  Result result = Result::kSuccess;
  if (transceiver_) {  // Unified Plan
    if (!transceiver_->sender()->SetTrack(local_track->impl())) {
      result = Result::kInvalidOperation;
    }
  } else {  // Plan B
    // auto ret = owner_->ReplaceTrackPlanB(local_track_, local_track);
    // if (!ret.ok()) {
    //  result = ret.error().result();
    //}
    result = Result::kUnknownError;  //< TODO
  }
  if (result != Result::kSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to set audio track " << local_track->GetName()
                      << " to audio transceiver " << GetName() << ".";
    return result;
  }
  local_track_ = std::move(local_track);
  local_track_->OnTransceiverChanged(this);
  return Result::kSuccess;
}

void AudioTransceiver::OnLocalTrackCreated(RefPtr<LocalAudioTrack> track) {
  RTC_DCHECK(!local_track_);
  local_track_ = std::move(track);
}

void AudioTransceiver::OnRemoteTrackCreated(RefPtr<RemoteAudioTrack> track) {
  RTC_DCHECK(!remote_track_);
  remote_track_ = std::move(track);
}

}  // namespace Microsoft::MixedReality::WebRTC

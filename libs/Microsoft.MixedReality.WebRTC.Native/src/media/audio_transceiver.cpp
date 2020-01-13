// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "audio_transceiver.h"
#include "interop/global_factory.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

AudioTransceiver::AudioTransceiver(
    PeerConnection& owner,
    mrsAudioTransceiverInteropHandle interop_handle) noexcept
    : Transceiver(MediaKind::kAudio, owner), interop_handle_(interop_handle) {
  GlobalFactory::Instance()->AddObject(ObjectType::kAudioTransceiver, this);
}

AudioTransceiver::AudioTransceiver(
    PeerConnection& owner,
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver,
    mrsAudioTransceiverInteropHandle interop_handle) noexcept
    : Transceiver(MediaKind::kAudio, owner, transceiver),
      interop_handle_(interop_handle) {
  GlobalFactory::Instance()->AddObject(ObjectType::kAudioTransceiver, this);
}

AudioTransceiver::~AudioTransceiver() {
  GlobalFactory::Instance()->RemoveObject(ObjectType::kAudioTransceiver, this);
}

Result AudioTransceiver::SetLocalTrack(
    RefPtr<LocalAudioTrack> local_track) noexcept {
  if (local_track_ == local_track) {
    return Result::kSuccess;
  }
  Result result = Result::kSuccess;
  if (transceiver_) {  // Unified Plan
    if (!transceiver_->sender()->SetTrack(local_track ? local_track->impl()
                                                      : nullptr)) {
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
    if (local_track) {
      RTC_LOG(LS_ERROR) << "Failed to set local audio track "
                        << local_track->GetName() << " of audio transceiver "
                        << GetName() << ".";
    } else {
      RTC_LOG(LS_ERROR)
          << "Failed to clear local audio track from audio transceiver "
          << GetName() << ".";
    }
    return result;
  }
  if (local_track_) {
    // Detach old local track
    local_track_->OnRemovedFromPeerConnection(*owner_, this,
                                              transceiver_->sender());
    owner_->OnLocalTrackRemovedFromAudioTransceiver(*this, *local_track_);
  }
  local_track_ = std::move(local_track);
  if (local_track_) {
    // Attach new local track
    local_track_->OnAddedToPeerConnection(*owner_, this,
                                          transceiver_->sender());
    owner_->OnLocalTrackAddedToAudioTransceiver(*this, *local_track_);
  }
  return Result::kSuccess;
}

void AudioTransceiver::OnLocalTrackAdded(RefPtr<LocalAudioTrack> track) {
  // this may be called multiple times with the same track
  RTC_DCHECK(!local_track_ || (local_track_ == track));
  local_track_ = std::move(track);
}

void AudioTransceiver::OnRemoteTrackAdded(RefPtr<RemoteAudioTrack> track) {
  // this may be called multiple times with the same track
  RTC_DCHECK(!remote_track_ || (remote_track_ == track));
  remote_track_ = std::move(track);
}

void AudioTransceiver::OnLocalTrackRemoved(LocalAudioTrack* track) {
  RTC_DCHECK_EQ(track, local_track_.get());
  local_track_ = nullptr;
}

void AudioTransceiver::OnRemoteTrackRemoved(RemoteAudioTrack* track) {
  RTC_DCHECK_EQ(track, remote_track_.get());
  remote_track_ = nullptr;
}

}  // namespace Microsoft::MixedReality::WebRTC
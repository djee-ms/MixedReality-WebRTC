// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "video_transceiver.h"

namespace Microsoft::MixedReality::WebRTC {

VideoTransceiver::VideoTransceiver(PeerConnection& owner) noexcept
    : Transceiver(MediaKind::kVideo, owner) {}

VideoTransceiver::VideoTransceiver(
    PeerConnection& owner,
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver,
    mrsVideoTransceiverInteropHandle interop_handle) noexcept
    : Transceiver(MediaKind::kVideo, owner, transceiver),
      interop_handle_(interop_handle) {}

VideoTransceiver::~VideoTransceiver() {}

Result VideoTransceiver::SetLocalTrack(
    RefPtr<LocalVideoTrack> local_track) noexcept {
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
    RTC_LOG(LS_ERROR) << "Failed to set video track " << local_track->GetName()
                      << " to video transceiver " << GetName() << ".";
    return result;
  }
  local_track_ = std::move(local_track);
  local_track_->OnTransceiverChanged(this);
  return Result::kSuccess;
}

void VideoTransceiver::OnLocalTrackAdded(RefPtr<LocalVideoTrack> track) {
  RTC_DCHECK(!local_track_);
  local_track_ = std::move(track);
}

void VideoTransceiver::OnRemoteTrackAdded(RefPtr<RemoteVideoTrack> track) {
  RTC_DCHECK(!remote_track_);
  remote_track_ = std::move(track);
}

void VideoTransceiver::OnLocalTrackRemoved(LocalVideoTrack* track) {
  RTC_DCHECK_EQ(track, local_track_.get());
  local_track_ = nullptr;
}

void VideoTransceiver::OnRemoteTrackRemoved(RemoteVideoTrack* track) {
  RTC_DCHECK_EQ(track, remote_track_.get());
  remote_track_ = nullptr;
}

}  // namespace Microsoft::MixedReality::WebRTC

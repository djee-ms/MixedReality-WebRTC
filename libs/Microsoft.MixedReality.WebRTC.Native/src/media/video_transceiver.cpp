// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"
#include "video_transceiver.h"

namespace Microsoft::MixedReality::WebRTC {

VideoTransceiver::VideoTransceiver(
    PeerConnection& owner,
    mrsVideoTransceiverInteropHandle interop_handle) noexcept
    : Transceiver(MediaKind::kVideo, owner), interop_handle_(interop_handle) {
  GlobalFactory::InstancePtr()->AddObject(ObjectType::kVideoTransceiver, this);
}

VideoTransceiver::VideoTransceiver(
    PeerConnection& owner,
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver,
    mrsVideoTransceiverInteropHandle interop_handle) noexcept
    : Transceiver(MediaKind::kVideo, owner, transceiver),
      interop_handle_(interop_handle) {
  GlobalFactory::InstancePtr()->AddObject(ObjectType::kVideoTransceiver, this);
}

VideoTransceiver::~VideoTransceiver() {
  // Be sure to clean-up WebRTC objects before unregistering ourself, which
  // could lead to the GlobalFactory being destroyed and the WebRTC threads
  // stopped.
  transceiver_ = nullptr;

  GlobalFactory::InstancePtr()->RemoveObject(ObjectType::kVideoTransceiver,
                                             this);
}

Result VideoTransceiver::SetDirection(Direction new_direction) noexcept {
  if (transceiver_) {  // Unified Plan
    if (new_direction == desired_direction_) {
      return Result::kSuccess;
    }
    transceiver_->SetDirection(ToRtp(new_direction));
  } else {  // Plan B
    //< TODO
    return Result::kUnknownError;
  }
  desired_direction_ = new_direction;
  FireStateUpdatedEvent(mrsTransceiverStateUpdatedReason::kSetDirection);
  return Result::kSuccess;
}

Result VideoTransceiver::SetLocalTrack(
    RefPtr<LocalVideoTrack> local_track) noexcept {
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
      RTC_LOG(LS_ERROR) << "Failed to set local video track "
                        << local_track->GetName() << " of video transceiver "
                        << GetName() << ".";
    } else {
      RTC_LOG(LS_ERROR)
          << "Failed to clear local video track from video transceiver "
          << GetName() << ".";
    }
    return result;
  }
  if (local_track_) {
    // Detach old local track
    owner_->OnLocalTrackRemovedFromVideoTransceiver(*this, *local_track_);
    local_track_->OnRemovedFromPeerConnection(*owner_, this,
                                              transceiver_->sender());
  }
  local_track_ = std::move(local_track);
  if (local_track_) {
    // Attach new local track
    local_track_->OnAddedToPeerConnection(*owner_, this,
                                          transceiver_->sender());
    owner_->OnLocalTrackAddedToVideoTransceiver(*this, *local_track_);
  }
  return Result::kSuccess;
}

void VideoTransceiver::OnLocalTrackAdded(RefPtr<LocalVideoTrack> track) {
  // this may be called multiple times with the same track
  RTC_DCHECK(!local_track_ || (local_track_ == track));
  local_track_ = std::move(track);
}

void VideoTransceiver::OnRemoteTrackAdded(RefPtr<RemoteVideoTrack> track) {
  // this may be called multiple times with the same track
  RTC_DCHECK(!remote_track_ || (remote_track_ == track));
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

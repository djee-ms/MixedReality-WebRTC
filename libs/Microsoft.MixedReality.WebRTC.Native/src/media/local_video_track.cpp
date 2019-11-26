// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "local_video_track_impl.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC::impl {

LocalVideoTrackImpl::LocalVideoTrackImpl(
    PeerConnection& owner,
    rtc::scoped_refptr<webrtc::VideoTrackInterface> track,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
    mrsLocalVideoTrackInteropHandle interop_handle) noexcept
    : owner_(&owner),
      track_(std::move(track)),
      sender_(std::move(sender)),
      interop_handle_(interop_handle) {
  RTC_CHECK(owner_);
  rtc::VideoSinkWants sink_settings{};
  sink_settings.rotation_applied = true;
  track_->AddOrUpdateSink(&observer_, sink_settings);
}

LocalVideoTrackImpl::~LocalVideoTrackImpl() {
  track_->RemoveSink(&observer_);
  if (owner_) {
    owner_->RemoveLocalVideoTrack(*this);
  }
  RTC_CHECK(!owner_);
}

str LocalVideoTrackImpl::GetName() const noexcept {
  return str(track_->id());
}

bool LocalVideoTrackImpl::IsEnabled() const noexcept {
  return track_->enabled();
}

void LocalVideoTrackImpl::SetEnabled(bool enabled) const noexcept {
  track_->set_enabled(enabled);
}

webrtc::VideoTrackInterface* LocalVideoTrackImpl::impl() const {
  return track_.get();
}

webrtc::RtpSenderInterface* LocalVideoTrackImpl::sender() const {
  return sender_.get();
}

void LocalVideoTrackImpl::RemoveFromPeerConnection(
    webrtc::PeerConnectionInterface& peer) {
  if (sender_) {
    peer.RemoveTrack(sender_);
    sender_ = nullptr;
    owner_ = nullptr;
  }
}

}  // namespace Microsoft::MixedReality::WebRTC::impl

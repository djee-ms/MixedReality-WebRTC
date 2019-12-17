// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "remote_video_track.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

RemoteVideoTrack::RemoteVideoTrack(
    PeerConnection& owner,
    rtc::scoped_refptr<webrtc::VideoTrackInterface> track,
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    mrsRemoteVideoTrackInteropHandle interop_handle) noexcept
    : MediaTrack(owner),
      track_(std::move(track)),
      receiver_(std::move(receiver)),
      interop_handle_(interop_handle) {
  RTC_CHECK(owner_);
  kind_ = TrackKind::kVideoTrack;
  rtc::VideoSinkWants sink_settings{};
  sink_settings.rotation_applied = true;
  track_->AddOrUpdateSink(this, sink_settings);
}

RemoteVideoTrack::~RemoteVideoTrack() {
  track_->RemoveSink(this);
  RTC_CHECK(!owner_);
}

std::string RemoteVideoTrack::GetName() const noexcept {
  return track_->id();
}

bool RemoteVideoTrack::IsEnabled() const noexcept {
  return track_->enabled();
}

void RemoteVideoTrack::SetEnabled(bool enabled) const noexcept {
  track_->set_enabled(enabled);
}

webrtc::VideoTrackInterface* RemoteVideoTrack::impl() const {
  return track_.get();
}

webrtc::RtpReceiverInterface* RemoteVideoTrack::receiver() const {
  return receiver_.get();
}

void RemoteVideoTrack::OnTrackRemoved(PeerConnection& owner) {
  RTC_DCHECK(owner_ == &owner);
  owner_ = nullptr;
}

}  // namespace Microsoft::MixedReality::WebRTC

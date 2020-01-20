// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"
#include "peer_connection.h"
#include "remote_video_track.h"

namespace Microsoft::MixedReality::WebRTC {

RemoteVideoTrack::RemoteVideoTrack(
    PeerConnection& owner,
    RefPtr<VideoTransceiver> transceiver,
    rtc::scoped_refptr<webrtc::VideoTrackInterface> track,
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    mrsRemoteVideoTrackInteropHandle interop_handle) noexcept
    : MediaTrack(owner),
      track_(std::move(track)),
      receiver_(std::move(receiver)),
      transceiver_(std::move(transceiver)),
      interop_handle_(interop_handle) {
  RTC_CHECK(owner_);
  RTC_CHECK(track_);
  RTC_CHECK(receiver_);
  RTC_CHECK(transceiver_);
  GlobalFactory::InstancePtr()->AddObject(ObjectType::kRemoteVideoTrack, this);
  kind_ = TrackKind::kVideoTrack;
  transceiver_->OnRemoteTrackAdded(this);
  rtc::VideoSinkWants sink_settings{};
  sink_settings.rotation_applied = true;
  track_->AddOrUpdateSink(this, sink_settings);
}

RemoteVideoTrack::~RemoteVideoTrack() {
  track_->RemoveSink(this);
  GlobalFactory::InstancePtr()->RemoveObject(ObjectType::kRemoteVideoTrack,
                                             this);
  RTC_CHECK(!owner_);
}

std::string RemoteVideoTrack::GetName() const noexcept {
  // Use stream ID #0 as track name for pairing with track on remote peer, as
  // track names are not guaranteed to be paired with Unified Plan (and were
  // actually neither with Plan B, but that worked in practice).
  auto ids = receiver_->stream_ids();
  if (!ids.empty()) {
    return ids[0];
  }
  // Fallback in case something went wrong, to help diagnose.
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

VideoTransceiver* RemoteVideoTrack::GetTransceiver() const {
  return transceiver_.get();
}

void RemoteVideoTrack::OnTrackRemoved(PeerConnection& owner) {
  RTC_DCHECK(owner_ == &owner);
  RTC_DCHECK(receiver_ != nullptr);
  RTC_DCHECK(transceiver_.get() != nullptr);
  owner_ = nullptr;
  receiver_ = nullptr;
  transceiver_->OnRemoteTrackRemoved(this);
  transceiver_ = nullptr;
}

}  // namespace Microsoft::MixedReality::WebRTC

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"
#include "local_video_track.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

LocalVideoTrack::LocalVideoTrack(
    PeerConnection& owner,
    RefPtr<VideoTransceiver> transceiver,
    rtc::scoped_refptr<webrtc::VideoTrackInterface> track,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
    mrsLocalVideoTrackInteropHandle interop_handle) noexcept
    : MediaTrack(owner),
      track_(std::move(track)),
      sender_(std::move(sender)),
      transceiver_(std::move(transceiver)),
      interop_handle_(interop_handle) {
  RTC_CHECK(owner_);
  RTC_CHECK(transceiver_);
  RTC_CHECK(track_);
  RTC_CHECK(sender_);
  GlobalFactory::Instance()->AddObject(ObjectType::kLocalVideoTrack, this);
  kind_ = TrackKind::kVideoTrack;
  transceiver_->OnLocalTrackAdded(this);
  rtc::VideoSinkWants sink_settings{};
  sink_settings.rotation_applied = true;
  track_->AddOrUpdateSink(this, sink_settings);
}

LocalVideoTrack::~LocalVideoTrack() {
  track_->RemoveSink(this);
  if (owner_) {
    owner_->RemoveLocalVideoTrack(*this);
  }
  GlobalFactory::Instance()->RemoveObject(ObjectType::kLocalVideoTrack, this);
  RTC_CHECK(!transceiver_);
  RTC_CHECK(!owner_);
}

std::string LocalVideoTrack::GetName() const noexcept {
  return track_->id();
}

void LocalVideoTrack::SetEnabled(bool enabled) const noexcept {
  track_->set_enabled(enabled);
}

bool LocalVideoTrack::IsEnabled() const noexcept {
  return track_->enabled();
}

RefPtr<VideoTransceiver> LocalVideoTrack::GetTransceiver() const noexcept {
  return transceiver_;
}

webrtc::VideoTrackInterface* LocalVideoTrack::impl() const {
  return track_.get();
}

webrtc::RtpSenderInterface* LocalVideoTrack::sender() const {
  return sender_.get();
}

void LocalVideoTrack::RemoveFromPeerConnection(
    webrtc::PeerConnectionInterface& peer) {
  if (sender_) {
    peer.RemoveTrack(sender_);
    sender_ = nullptr;
    owner_ = nullptr;
    transceiver_->OnLocalTrackRemoved(this);
    transceiver_ = nullptr;
  }
}

void LocalVideoTrack::OnTransceiverChanged(
    RefPtr<VideoTransceiver> newTransceiver) {
  RTC_CHECK_NE(transceiver_.get(), newTransceiver.get());
  RTC_CHECK_NE(transceiver_.get(), (VideoTransceiver*)nullptr);
  transceiver_->OnLocalTrackRemoved(this);
  transceiver_ = newTransceiver;
  transceiver_->OnLocalTrackAdded(this);
}

}  // namespace Microsoft::MixedReality::WebRTC

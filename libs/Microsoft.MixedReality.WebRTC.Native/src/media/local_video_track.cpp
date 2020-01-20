// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"
#include "local_video_track.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

LocalVideoTrack::LocalVideoTrack(
    rtc::scoped_refptr<webrtc::VideoTrackInterface> track,
    mrsLocalVideoTrackInteropHandle interop_handle) noexcept
    : MediaTrack(), track_(std::move(track)), interop_handle_(interop_handle) {
  RTC_CHECK(track_);
  GlobalFactory::InstancePtr()->AddObject(ObjectType::kLocalVideoTrack, this);
  kind_ = TrackKind::kVideoTrack;
  rtc::VideoSinkWants sink_settings{};
  sink_settings.rotation_applied = true;
  track_->AddOrUpdateSink(this, sink_settings);
}

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
  GlobalFactory::InstancePtr()->AddObject(ObjectType::kLocalVideoTrack, this);
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
  GlobalFactory::InstancePtr()->RemoveObject(ObjectType::kLocalVideoTrack,
                                             this);
  RTC_CHECK(!transceiver_);
  RTC_CHECK(!owner_);
}

std::string LocalVideoTrack::GetName() const noexcept {
  // Use stream ID #0 as track name for pairing with track on remote peer, as
  // track names are not guaranteed to be paired with Unified Plan (and were
  // actually neither with Plan B, but that worked in practice).
  auto ids = sender_->stream_ids();
  if (!ids.empty()) {
    return ids[0];
  }
  // Fallback in case something went wrong, to help diagnose.
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

void LocalVideoTrack::OnAddedToPeerConnection(
    PeerConnection& owner,
    RefPtr<VideoTransceiver> transceiver,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender) {
  RTC_CHECK(!owner_);
  RTC_CHECK(!transceiver_);
  RTC_CHECK(!sender_);
  RTC_CHECK(transceiver);
  RTC_CHECK(sender);
  owner_ = &owner;
  sender_ = std::move(sender);
  transceiver_ = std::move(transceiver);
  transceiver_->OnLocalTrackAdded(this);
}

void LocalVideoTrack::OnRemovedFromPeerConnection(
    PeerConnection& old_owner,
    RefPtr<VideoTransceiver> old_transceiver,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> old_sender) {
  RTC_CHECK_EQ(owner_, &old_owner);
  RTC_CHECK_EQ(transceiver_.get(), old_transceiver.get());
  RTC_CHECK_EQ(sender_.get(), old_sender.get());
  owner_ = nullptr;
  sender_ = nullptr;
  transceiver_->OnLocalTrackRemoved(this);
  transceiver_ = nullptr;
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

}  // namespace Microsoft::MixedReality::WebRTC

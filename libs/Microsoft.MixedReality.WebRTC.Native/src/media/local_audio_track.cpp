// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"
#include "local_audio_track.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

LocalAudioTrack::LocalAudioTrack(
    rtc::scoped_refptr<webrtc::AudioTrackInterface> track,
    mrsLocalAudioTrackInteropHandle interop_handle) noexcept
    : MediaTrack(), track_(std::move(track)), interop_handle_(interop_handle) {
  RTC_CHECK(track_);
  GlobalFactory::InstancePtr()->AddObject(ObjectType::kLocalAudioTrack, this);
  kind_ = TrackKind::kAudioTrack;
  track_->AddSink(this);  //< FIXME - Implementation is no-op
}

LocalAudioTrack::LocalAudioTrack(
    PeerConnection& owner,
    RefPtr<AudioTransceiver> transceiver,
    rtc::scoped_refptr<webrtc::AudioTrackInterface> track,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
    mrsLocalAudioTrackInteropHandle interop_handle) noexcept
    : MediaTrack(owner),
      track_(std::move(track)),
      sender_(std::move(sender)),
      transceiver_(std::move(transceiver)),
      interop_handle_(interop_handle) {
  RTC_CHECK(owner_);
  RTC_CHECK(transceiver_);
  RTC_CHECK(track_);
  RTC_CHECK(sender_);
  GlobalFactory::InstancePtr()->AddObject(ObjectType::kLocalAudioTrack, this);
  kind_ = TrackKind::kAudioTrack;
  transceiver_->OnLocalTrackAdded(this);
  track_->AddSink(this);  //< FIXME - Implementation is no-op
}

LocalAudioTrack::~LocalAudioTrack() {
  track_->RemoveSink(this);
  if (owner_) {
    owner_->RemoveLocalAudioTrack(*this);
  }
  GlobalFactory::InstancePtr()->RemoveObject(ObjectType::kLocalAudioTrack,
                                             this);
  RTC_CHECK(!transceiver_);
  RTC_CHECK(!owner_);
}

std::string LocalAudioTrack::GetName() const noexcept {
  // Use stream ID #0 as track name for pairing with track on remote peer, as
  // track names are not guaranteed to be paired with Unified Plan (and were
  // actually neither with Plan B, but that worked in practice).
  if (sender_) {
    auto ids = sender_->stream_ids();
    if (!ids.empty()) {
      return ids[0];
    }
  }
  // Fallback in case something went wrong, to help diagnose.
  return track_->id();
}

void LocalAudioTrack::SetEnabled(bool enabled) const noexcept {
  track_->set_enabled(enabled);
}

bool LocalAudioTrack::IsEnabled() const noexcept {
  return track_->enabled();
}

RefPtr<AudioTransceiver> LocalAudioTrack::GetTransceiver() const noexcept {
  return transceiver_;
}

webrtc::AudioTrackInterface* LocalAudioTrack::impl() const {
  return track_.get();
}

webrtc::RtpSenderInterface* LocalAudioTrack::sender() const {
  return sender_.get();
}

void LocalAudioTrack::OnAddedToPeerConnection(
    PeerConnection& owner,
    RefPtr<AudioTransceiver> transceiver,
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

void LocalAudioTrack::OnRemovedFromPeerConnection(
    PeerConnection& old_owner,
    RefPtr<AudioTransceiver> old_transceiver,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> old_sender) {
  RTC_CHECK_EQ(owner_, &old_owner);
  RTC_CHECK_EQ(transceiver_.get(), old_transceiver.get());
  RTC_CHECK_EQ(sender_.get(), old_sender.get());
  owner_ = nullptr;
  sender_ = nullptr;
  transceiver_->OnLocalTrackRemoved(this);
  transceiver_ = nullptr;
}

void LocalAudioTrack::RemoveFromPeerConnection(
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

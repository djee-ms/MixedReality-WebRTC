// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "local_audio_track.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

LocalAudioTrack::LocalAudioTrack(
    PeerConnection& owner,
    rtc::scoped_refptr<webrtc::AudioTrackInterface> track,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
    mrsLocalAudioTrackInteropHandle interop_handle) noexcept
    : MediaTrack(owner),
      track_(std::move(track)),
      sender_(std::move(sender)),
      interop_handle_(interop_handle) {
  RTC_CHECK(owner_);
  kind_ = TrackKind::kAudioTrack;
  track_->AddSink(this);  //< FIXME - Implementation is no-op
}

LocalAudioTrack::~LocalAudioTrack() {
  track_->RemoveSink(this);
  if (owner_) {
    owner_->RemoveLocalAudioTrack(*this);
  }
  RTC_CHECK(!owner_);
}

std::string LocalAudioTrack::GetName() const noexcept {
  return track_->id();
}

bool LocalAudioTrack::IsEnabled() const noexcept {
  return track_->enabled();
}

void LocalAudioTrack::SetEnabled(bool enabled) const noexcept {
  track_->set_enabled(enabled);
}

webrtc::AudioTrackInterface* LocalAudioTrack::impl() const {
  return track_.get();
}

webrtc::RtpSenderInterface* LocalAudioTrack::sender() const {
  return sender_.get();
}

void LocalAudioTrack::RemoveFromPeerConnection(
    webrtc::PeerConnectionInterface& peer) {
  if (sender_) {
    peer.RemoveTrack(sender_);
    sender_ = nullptr;
    owner_ = nullptr;
  }
}

}  // namespace Microsoft::MixedReality::WebRTC

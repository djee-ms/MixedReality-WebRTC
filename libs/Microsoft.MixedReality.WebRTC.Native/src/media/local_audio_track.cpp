// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"
#include "local_audio_track.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

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
  GlobalFactory::Instance()->AddObject(ObjectType::kLocalAudioTrack, this);
  kind_ = TrackKind::kAudioTrack;
  transceiver_->OnLocalTrackAdded(this);
  track_->AddSink(this);  //< FIXME - Implementation is no-op
}

LocalAudioTrack::~LocalAudioTrack() {
  track_->RemoveSink(this);
  if (owner_) {
    owner_->RemoveLocalAudioTrack(*this);
  }
  GlobalFactory::Instance()->RemoveObject(ObjectType::kLocalAudioTrack, this);
  RTC_CHECK(!transceiver_);
  RTC_CHECK(!owner_);
}

std::string LocalAudioTrack::GetName() const noexcept {
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

void LocalAudioTrack::OnTransceiverChanged(
    RefPtr<AudioTransceiver> newTransceiver) {
  RTC_CHECK_NE(transceiver_.get(), newTransceiver.get());
  RTC_CHECK_NE(transceiver_.get(), (AudioTransceiver*)nullptr);
  transceiver_->OnLocalTrackRemoved(this);
  transceiver_ = newTransceiver;
  transceiver_->OnLocalTrackAdded(this);
}

}  // namespace Microsoft::MixedReality::WebRTC

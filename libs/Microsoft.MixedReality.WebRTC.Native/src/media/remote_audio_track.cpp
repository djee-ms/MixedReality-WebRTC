// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"
#include "peer_connection.h"
#include "remote_audio_track.h"

namespace Microsoft::MixedReality::WebRTC {

RemoteAudioTrack::RemoteAudioTrack(
    PeerConnection& owner,
    RefPtr<AudioTransceiver> transceiver,
    rtc::scoped_refptr<webrtc::AudioTrackInterface> track,
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    mrsRemoteAudioTrackInteropHandle interop_handle) noexcept
    : MediaTrack(owner),
      track_(std::move(track)),
      receiver_(std::move(receiver)),
      transceiver_(std::move(transceiver)),
      interop_handle_(interop_handle) {
  RTC_CHECK(owner_);
  RTC_CHECK(track_);
  RTC_CHECK(receiver_);
  RTC_CHECK(transceiver_);
  GlobalFactory::Instance()->AddObject(ObjectType::kRemoteAudioTrack, this);
  kind_ = TrackKind::kAudioTrack;
  transceiver_->OnRemoteTrackAdded(this);
  track_->AddSink(this);
}

RemoteAudioTrack::~RemoteAudioTrack() {
  track_->RemoveSink(this);
  GlobalFactory::Instance()->RemoveObject(ObjectType::kRemoteAudioTrack, this);
  RTC_CHECK(!owner_);
}

webrtc::AudioTrackInterface* RemoteAudioTrack::impl() const {
  return track_.get();
}

webrtc::RtpReceiverInterface* RemoteAudioTrack::receiver() const {
  return receiver_.get();
}

AudioTransceiver* RemoteAudioTrack::GetTransceiver() const {
  return transceiver_.get();
}

void RemoteAudioTrack::OnTrackRemoved(PeerConnection& owner) {
  RTC_DCHECK(owner_ == &owner);
  RTC_DCHECK(receiver_ != nullptr);
  RTC_DCHECK(transceiver_.get() != nullptr);
  owner_ = nullptr;
  receiver_ = nullptr;
  transceiver_->OnRemoteTrackRemoved(this);
  transceiver_ = nullptr;
}

}  // namespace Microsoft::MixedReality::WebRTC

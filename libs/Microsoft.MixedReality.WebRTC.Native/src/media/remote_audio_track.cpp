// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "peer_connection.h"
#include "remote_audio_track.h"

namespace Microsoft::MixedReality::WebRTC {

RemoteAudioTrack::RemoteAudioTrack(
    PeerConnection& owner,
    rtc::scoped_refptr<webrtc::AudioTrackInterface> track,
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    mrsRemoteAudioTrackInteropHandle interop_handle) noexcept
    : MediaTrack(owner),
      track_(std::move(track)),
      receiver_(std::move(receiver)),
      interop_handle_(interop_handle) {
  RTC_CHECK(owner_);
  kind_ = TrackKind::kAudioTrack;
  track_->AddSink(this);
}

RemoteAudioTrack::~RemoteAudioTrack() {
  track_->RemoveSink(this);
  RTC_CHECK(!owner_);
}

void RemoteAudioTrack::OnTrackRemoved(PeerConnection& owner) {
  RTC_DCHECK(owner_ == &owner);
  owner_ = nullptr;
}

}  // namespace Microsoft::MixedReality::WebRTC

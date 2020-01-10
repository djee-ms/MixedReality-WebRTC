// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "peer_connection.h"
#include "transceiver.h"

namespace Microsoft::MixedReality::WebRTC {

Transceiver::Transceiver(MediaKind kind, PeerConnection& owner) noexcept
    : owner_(&owner), kind_(kind) {
  RTC_CHECK(owner_);
  //< TODO
  // RTC_CHECK(owner.sdp_semantic == webrtc::SdpSemantics::kPlanB);
}

Transceiver::Transceiver(
    MediaKind kind,
    PeerConnection& owner,
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) noexcept
    : owner_(&owner), kind_(kind), transceiver_(std::move(transceiver)) {
  RTC_CHECK(owner_);
  //< TODO
  // RTC_CHECK(owner.sdp_semantic == webrtc::SdpSemantics::kUnifiedPlan);
}

Transceiver::~Transceiver() {
  // RTC_CHECK(!owner_);
}

std::string Transceiver::GetName() const {
  return "TODO";
}

rtc::scoped_refptr<webrtc::RtpTransceiverInterface> Transceiver::impl() const {
  return transceiver_;
}

}  // namespace Microsoft::MixedReality::WebRTC

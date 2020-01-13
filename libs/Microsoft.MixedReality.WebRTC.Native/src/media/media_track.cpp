// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "media_track.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

MediaTrack::MediaTrack() noexcept = default;

MediaTrack::MediaTrack(PeerConnection& owner) noexcept : owner_(&owner) {
  RTC_CHECK(owner_);
}

MediaTrack::~MediaTrack() {
  RTC_CHECK(!owner_);
}

}  // namespace Microsoft::MixedReality::WebRTC

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include <mrwebrtcxx/local_video_track.h>
#include <mrwebrtcxx/peer_connection.h>

namespace Microsoft::MixedReality::WebRTC {

LocalVideoTrack::LocalVideoTrack(PeerConnection& owner,
                                 mrsLocalVideoTrackHandle handle)
    : owner_(&owner), handle_(handle) {
  assert(owner_);
}

LocalVideoTrack::~LocalVideoTrack() {
  if (owner_) {
    owner_->RemoveLocalVideoTrack(*this);
  }
  assert(!owner_);
}

bool LocalVideoTrack::IsEnabled() const noexcept {
  return (mrsLocalVideoTrackIsEnabled(GetHandle()) != mrsBool::kFalse);
}

void LocalVideoTrack::SetEnabled(bool enabled) const {
  CheckResult(mrsLocalVideoTrackSetEnabled(
      GetHandle(), enabled ? mrsBool::kTrue : mrsBool::kFalse));
}

}  // namespace Microsoft::MixedReality::WebRTC

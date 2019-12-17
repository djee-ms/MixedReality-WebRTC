// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "callback.h"
#include "interop/interop_api.h"
#include "str.h"
#include "tracked_object.h"
#include "video_frame_observer.h"

namespace rtc {
template <typename T>
class scoped_refptr;
}

namespace webrtc {
class RtpSenderInterface;
class VideoTrackInterface;
}  // namespace webrtc

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;

/// Base class for all audio and video tracks.
class MediaTrack : public TrackedObject {
 public:
  MediaTrack(PeerConnection& owner) noexcept;
  MRS_API ~MediaTrack() override;

  /// Get the kind of track.
  MRS_API TrackKind GetKind() const noexcept { return kind_; }

 protected:
  /// Weak reference to the PeerConnection object owning this track.
  PeerConnection* owner_{};

  /// Track kind.
  TrackKind kind_ = TrackKind::kUnknownTrack;
};

}  // namespace Microsoft::MixedReality::WebRTC

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "callback.h"
#include "interop/interop_api.h"
#include "tracked_object.h"

namespace rtc {
template <typename T>
class scoped_refptr;
}

namespace webrtc {
class RtpTransceiverInterface;
}

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;

/// Media kind for tracks and transceivers.
enum class MediaKind : uint32_t { kAudio, kVideo };

/// Base class for audio and video transceivers.
class Transceiver : public TrackedObject {
 public:
  /// Flow direction of the media inside the transceiver. This maps to whether
  /// local and/or remote tracks are attached to the transceiver. The local
  /// track corresponds to the send direction, and the remote track to the
  /// receive direction.
  enum class Direction { kSendRecv, kSendOnly, kRecvOnly, kInactive };

  /// Construct a Plan B transceiver abstraction which tries to mimic a
  /// transceiver for Plan B despite the fact that this semantic doesn't have
  /// any concept of transceiver.
  Transceiver(MediaKind kind, PeerConnection& owner) noexcept;

  /// Construct a Unified Plan transceiver wrapper referencing an actual WebRTC
  /// transceiver implementation object as defined in Unified Plan.
  Transceiver(
      MediaKind kind,
      PeerConnection& owner,
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) noexcept;

  MRS_API ~Transceiver() override;

  std::string GetName() const override;

  /// Get the kind of transceiver. This is generally used for determining what
  /// type to static_cast<> a |Transceiver| pointer to. If this is |kAudio| then
  /// the object is an |AudioTransceiver| instance, and if this is |kVideo| then
  /// the object is a |VideoTransceiver| instance.
  MRS_API MediaKind GetKind() const noexcept { return kind_; }

  /// Get the current transceiver direction.
  MRS_API Direction GetDirection() const noexcept { return direction_; }

  //
  // Advanced
  //

  [[nodiscard]] rtc::scoped_refptr<webrtc::RtpTransceiverInterface> impl()
      const;

 protected:
  /// Weak reference to the PeerConnection object owning this transceiver.
  PeerConnection* owner_{};

  /// Transceiver media kind.
  MediaKind kind_;

  /// Current transceiver direction, as last negotiated.
  /// This does not map 1:1 with the presence/absence of the local and remote
  /// tracks in |AudioTransceiver| and |VideoTransceiver|, which represent the
  /// state for the next negotiation, and will differ after changing tracks but
  /// before renegotiating them. This does map however with the internal concept
  /// of "preferred direction" |webrtc::RtpTransceiverInterface::direction()|.
  Direction direction_ = Direction::kInactive;

  /// If the owner PeerConnection uses Unified Plan, pointer to the actual
  /// transceiver implementation object. Otherwise NULL for Plan B.
  rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver_;
};

}  // namespace Microsoft::MixedReality::WebRTC

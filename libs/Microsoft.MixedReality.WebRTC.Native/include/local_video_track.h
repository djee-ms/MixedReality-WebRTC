// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "tracked_object.h"
#include "video_frame.h"

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;

/// A local video track is a media track for a peer connection backed by a local
/// source, and transmitted to a remote peer.
///
/// The local nature of the track implies that the local peer has control on it,
/// including enabling or disabling the track, and removing it from the peer
/// connection. This is in contrast with a remote track	reflecting a track sent
/// by the remote peer, for which the local peer has limited control.
///
/// The local video track is backed by a local video track source. This is
/// typically a video capture device (e.g. webcam), but can	also be a source
/// producing programmatically generated frames. The local video track itself
/// has no knowledge about how the source produces the frames.
class LocalVideoTrack : public TrackedObject {
 public:
  /// Enable or disable the video track. An enabled track streams its content
  /// from its source to the remote peer. A disabled video track only sends
  /// black frames.
  virtual void SetEnabled(bool enabled) const noexcept = 0;

  /// Check if the track is enabled.
  /// See |SetEnabled(bool)|.
  [[nodiscard]] virtual bool IsEnabled() const noexcept = 0;

  /// Register a callback to get notified on frame available,
  /// and received that frame as a I420-encoded buffer.
  /// This is not exclusive and can be used along another ARGB callback.
  virtual void SetCallback(I420AFrameReadyCallback callback) noexcept = 0;

  /// Register a callback to get notified on frame available,
  /// and received that frame as a raw decoded ARGB buffer.
  /// This is not exclusive and can be used along another I420 callback.
  virtual void SetCallback(ARGBFrameReadyCallback callback) noexcept = 0;
};

}  // namespace Microsoft::MixedReality::WebRTC

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "callback.h"

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
class LocalVideoTrack {
 public:
  LocalVideoTrack(PeerConnection& owner, LocalVideoTrackHandle handle);
  virtual ~LocalVideoTrack();

  /// Enable or disable the video track. An enabled track streams its content
  /// from its source to the remote peer. A disabled video track only sends
  /// black frames.
  void SetEnabled(bool enabled) const;

  /// Check if the track is enabled.
  /// See |SetEnabled(bool)|.
  [[nodiscard]] bool IsEnabled() const noexcept;

  //
  // Errors
  //

  /// Helper method invoked for each API call, to check that the call was
  /// successful. This default implementation throws an exception depending on
  /// the type of error in |result|.
  virtual void CheckResult(mrsResult result) const { ThrowOnError(result); }

  //
  // Advanced use
  //

  /// Get the handle to the implementation object, to make an API call.
  /// This throws an |InvalidOperationException| when the handle is not valid.
  [[nodiscard]] LocalVideoTrackHandle GetHandle() const {
    if (!handle_) {
      throw new InvalidOperationException();
    }
    return handle_;
  }

 private:
  /// Weak reference to the PeerConnection object owning this track.
  PeerConnection* owner_{};

  /// Handle to the implementation object.
  LocalVideoTrackHandle handle_{};
};

}  // namespace Microsoft::MixedReality::WebRTC

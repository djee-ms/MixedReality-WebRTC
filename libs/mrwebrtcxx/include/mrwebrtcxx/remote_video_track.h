// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <mrwebrtcxx/callback.h>
#include <mrwebrtcxx/exceptions.h>

#include <mrwebrtc/remote_video_track_interop.h>
#include <mrwebrtc/result.h>

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;

/// A remote video track is a media track for a peer connection backed by a
/// remote source, and receiving video frames from a remote peer.
class RemoteVideoTrack {
 public:
  using I420AFrameReadyCallback = Callback<const I420AVideoFrame&>;
  using Argb32FrameReadyCallback = Callback<const Argb32VideoFrame&>;

  /// Register a custom callback invoked when a video frame has been received
  /// and decompressed, and is ready to be used.
  void RegisterI420AFrameReadyCallback(
      I420AFrameReadyCallback&& callback) noexcept {
    i420a_cb_ = std::move(callback);
    if (i420a_cb_) {
      mrsRemoteVideoTrackRegisterI420AFrameCallback(
          GetHandle(), &I420AFrameReadyCallback::StaticExec,
          i420a_cb_.GetUserData());
    } else {
      mrsRemoteVideoTrackRegisterI420AFrameCallback(GetHandle(), nullptr,
                                                    nullptr);
    }
  }

  /// Register a custom callback invoked when a video frame has been received
  /// and decompressed, and is ready to be used.
  void RegisterArgb32FrameReadyCallback(
      Argb32FrameReadyCallback&& callback) noexcept {
    argb32_cb_ = std::move(callback);
    if (argb32_cb_) {
      mrsRemoteVideoTrackRegisterArgb32FrameCallback(
          GetHandle(), &Argb32FrameReadyCallback::StaticExec,
          argb32_cb_.GetUserData());
    } else {
      mrsRemoteVideoTrackRegisterArgb32FrameCallback(GetHandle(), nullptr,
                                                     nullptr);
    }
  }

  /// Check if the track is enabled.
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
  [[nodiscard]] mrsRemoteVideoTrackHandle GetHandle() const {
    if (!handle_) {
      throw new InvalidOperationException();
    }
    return handle_;
  }

 private:
  /// Handle to the implementation object.
  mrsRemoteVideoTrackHandle handle_{};

  I420AFrameReadyCallback i420a_cb_;
  Argb32FrameReadyCallback argb32_cb_;

  /// Weak reference to the PeerConnection object owning this track.
  PeerConnection* owner_{};
};

}  // namespace Microsoft::MixedReality::WebRTC

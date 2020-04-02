// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <mrwebrtcxx/callback.h>
#include <mrwebrtcxx/exceptions.h>

#include <mrwebrtc/remote_audio_track_interop.h>
#include <mrwebrtc/result.h>

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;

class RemoteAudioTrack {
 public:
  using AudioFrameReadyCallback = Callback<const AudioFrame&>;

  /// Register a custom callback invoked when an audio frame has been received
  /// and decompressed, and is ready to be used.
  void RegisterFrameReadyCallback(AudioFrameReadyCallback&& callback) noexcept {
    audio_cb_ = std::move(callback);
    if (audio_cb_) {
      mrsRemoteAudioTrackRegisterFrameCallback(
          GetHandle(), &AudioFrameReadyCallback::StaticExec,
          audio_cb_.GetUserData());
    } else {
      mrsRemoteAudioTrackRegisterFrameCallback(GetHandle(), nullptr, nullptr);
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
  [[nodiscard]] mrsRemoteAudioTrackHandle GetHandle() const {
    if (!handle_) {
      throw new InvalidOperationException();
    }
    return handle_;
  }

 private:
  /// Handle to the implementation object.
  mrsRemoteAudioTrackHandle handle_{};

  AudioFrameReadyCallback audio_cb_;

  /// Weak reference to the PeerConnection object owning this track.
  PeerConnection* owner_{};
};

}  // namespace Microsoft::MixedReality::WebRTC

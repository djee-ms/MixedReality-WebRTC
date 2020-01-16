// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "callback.h"
#include "interop/interop_api.h"
#include "media/local_audio_track.h"
#include "media/remote_audio_track.h"
#include "media/transceiver.h"
#include "refptr.h"

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;

/// Transceiver containing audio tracks.
class AudioTransceiver : public Transceiver {
 public:
  /// Constructor for Plan B.
  AudioTransceiver(PeerConnection& owner,
                   mrsAudioTransceiverInteropHandle interop_handle) noexcept;

  /// Constructor for Unified Plan.
  AudioTransceiver(
      PeerConnection& owner,
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver,
      mrsAudioTransceiverInteropHandle interop_handle) noexcept;
  MRS_API ~AudioTransceiver() override;

  Result SetDirection(Direction new_direction) noexcept;

  MRS_API Result SetLocalTrack(RefPtr<LocalAudioTrack> local_track) noexcept;

  MRS_API RefPtr<LocalAudioTrack> GetLocalTrack() noexcept {
    return local_track_;
  }

  MRS_API RefPtr<RemoteAudioTrack> GetRemoteTrack() noexcept {
    return remote_track_;
  }

  //
  // Internal
  //

  void OnLocalTrackAdded(RefPtr<LocalAudioTrack> track);
  void OnRemoteTrackAdded(RefPtr<RemoteAudioTrack> track);
  void OnLocalTrackRemoved(LocalAudioTrack* track);
  void OnRemoteTrackRemoved(RemoteAudioTrack* track);
  mrsAudioTransceiverInteropHandle GetInteropHandle() const {
    return interop_handle_;
  }

 protected:
  RefPtr<LocalAudioTrack> local_track_;
  RefPtr<RemoteAudioTrack> remote_track_;

  /// Optional interop handle, if associated with an interop wrapper.
  mrsAudioTransceiverInteropHandle interop_handle_{};
};

}  // namespace Microsoft::MixedReality::WebRTC

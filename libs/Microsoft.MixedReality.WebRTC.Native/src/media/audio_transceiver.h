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
  AudioTransceiver(PeerConnection& owner) noexcept;
  AudioTransceiver(
      PeerConnection& owner,
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) noexcept;
  MRS_API ~AudioTransceiver() override;

  MRS_API Result SetLocalTrack(RefPtr<LocalAudioTrack> local_track) noexcept;

  MRS_API RefPtr<LocalAudioTrack> GetLocalTrack() noexcept {
    return local_track_;
  }

  MRS_API RefPtr<RemoteAudioTrack> GetRemoteTrack() noexcept {
    return remote_track_;
  }

  //
  // Advanced
  //

  void OnLocalTrackCreated(RefPtr<LocalAudioTrack> track);

 protected:
  RefPtr<LocalAudioTrack> local_track_;
  RefPtr<RemoteAudioTrack> remote_track_;
};

}  // namespace Microsoft::MixedReality::WebRTC

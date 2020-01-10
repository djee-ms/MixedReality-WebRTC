// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "callback.h"
#include "interop/interop_api.h"
#include "local_video_track.h"
#include "media/remote_video_track.h"
#include "media/transceiver.h"
#include "refptr.h"

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;

/// Transceiver containing video tracks.
class VideoTransceiver : public Transceiver {
 public:
  VideoTransceiver(PeerConnection& owner) noexcept;
  VideoTransceiver(
      PeerConnection& owner,
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) noexcept;
  MRS_API ~VideoTransceiver() override;

  MRS_API Result SetLocalTrack(RefPtr<LocalVideoTrack> local_track) noexcept;

  MRS_API RefPtr<LocalVideoTrack> GetLocalTrack() noexcept {
    return local_track_;
  }

  MRS_API RefPtr<RemoteVideoTrack> GetRemoteTrack() noexcept {
    return remote_track_;
  }

  //
  // Advanced
  //

  void OnLocalTrackCreated(RefPtr<LocalVideoTrack> track);

 protected:
  RefPtr<LocalVideoTrack> local_track_;
  RefPtr<RemoteVideoTrack> remote_track_;
};

}  // namespace Microsoft::MixedReality::WebRTC

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
  /// Constructor for Plan B.
  VideoTransceiver(PeerConnection& owner,
                   mrsVideoTransceiverInteropHandle interop_handle) noexcept;

  /// Constructor for Unified Plan.
  VideoTransceiver(
      PeerConnection& owner,
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver,
      mrsVideoTransceiverInteropHandle interop_handle) noexcept;

  MRS_API ~VideoTransceiver() override;

  Result SetDirection(Direction new_direction) noexcept;

  MRS_API Result SetLocalTrack(RefPtr<LocalVideoTrack> local_track) noexcept;

  MRS_API RefPtr<LocalVideoTrack> GetLocalTrack() noexcept {
    return local_track_;
  }

  MRS_API RefPtr<RemoteVideoTrack> GetRemoteTrack() noexcept {
    return remote_track_;
  }

  //
  // Internal
  //

  void OnLocalTrackAdded(RefPtr<LocalVideoTrack> track);
  void OnRemoteTrackAdded(RefPtr<RemoteVideoTrack> track);
  void OnLocalTrackRemoved(LocalVideoTrack* track);
  void OnRemoteTrackRemoved(RemoteVideoTrack* track);
  mrsVideoTransceiverInteropHandle GetInteropHandle() const {
    return interop_handle_;
  }

 protected:
  RefPtr<LocalVideoTrack> local_track_;
  RefPtr<RemoteVideoTrack> remote_track_;

  /// Optional interop handle, if associated with an interop wrapper.
  mrsVideoTransceiverInteropHandle interop_handle_{};
};

}  // namespace Microsoft::MixedReality::WebRTC

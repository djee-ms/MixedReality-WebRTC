// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "callback.h"
#include "interop/interop_api.h"
#include "media/media_track.h"
#include "refptr.h"
#include "tracked_object.h"
#include "audio_frame_observer.h"

namespace rtc {
template <typename T>
class scoped_refptr;
}

namespace webrtc {
class RtpSenderInterface;
class AudioTrackInterface;
}  // namespace webrtc

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;
class AudioTransceiver;

/// A local audio track is a media track for a peer connection backed by a local
/// source, and transmitted to a remote peer.
///
/// The local nature of the track implies that the local peer has control on it,
/// including enabling or disabling the track, and removing it from the peer
/// connection. This is in contrast with a remote track	reflecting a track sent
/// by the remote peer, for which the local peer has limited control.
///
/// The local audio track is backed by a local audio track source. This is
/// typically a audio capture device (e.g. webcam), but can	also be a source
/// producing programmatically generated frames. The local audio track itself
/// has no knowledge about how the source produces the frames.
class LocalAudioTrack : public AudioFrameObserver, public MediaTrack {
 public:
  LocalAudioTrack(PeerConnection& owner,
                  RefPtr<AudioTransceiver> transceiver,
                  rtc::scoped_refptr<webrtc::AudioTrackInterface> track,
                  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
                  mrsLocalAudioTrackInteropHandle interop_handle) noexcept;
  MRS_API ~LocalAudioTrack() override;

  /// Get the name of the local audio track.
  MRS_API std::string GetName() const noexcept override;

  /// Enable or disable the audio track. An enabled track streams its content
  /// from its source to the remote peer. A disabled audio track only sends
  /// black frames.
  MRS_API void SetEnabled(bool enabled) const noexcept;

  /// Check if the track is enabled.
  /// See |SetEnabled(bool)|.
  MRS_API [[nodiscard]] bool IsEnabled() const noexcept;

  [[nodiscard]] MRS_API RefPtr<AudioTransceiver> GetTransceiver() const
      noexcept;

  //
  // Advanced use
  //

  [[nodiscard]] webrtc::AudioTrackInterface* impl() const;
  [[nodiscard]] webrtc::RtpSenderInterface* sender() const;

  [[nodiscard]] mrsLocalAudioTrackInteropHandle GetInteropHandle() const
      noexcept {
    return interop_handle_;
  }

  void RemoveFromPeerConnection(webrtc::PeerConnectionInterface& peer);

  void OnTransceiverChanged(RefPtr<AudioTransceiver> newTransceiver);

 private:
  /// Underlying core implementation.
  rtc::scoped_refptr<webrtc::AudioTrackInterface> track_;

  /// RTP sender this track is associated with.
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender_;

  /// Transceiver this track is associated with, if any.
  RefPtr<AudioTransceiver> transceiver_;

  /// Optional interop handle, if associated with an interop wrapper.
  mrsLocalAudioTrackInteropHandle interop_handle_{};
};

}  // namespace Microsoft::MixedReality::WebRTC

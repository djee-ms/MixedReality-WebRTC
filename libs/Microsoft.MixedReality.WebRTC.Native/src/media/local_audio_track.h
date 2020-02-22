// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "audio_frame_observer.h"
#include "callback.h"
#include "interop_api.h"
#include "media/media_track.h"
#include "refptr.h"
#include "tracked_object.h"

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
  /// Constructor for a track not added to any peer connection.
  LocalAudioTrack(rtc::scoped_refptr<webrtc::AudioTrackInterface> track,
                  mrsLocalAudioTrackInteropHandle interop_handle) noexcept;

  /// Constructor for a track added to a peer connection.
  LocalAudioTrack(PeerConnection& owner,
                  RefPtr<AudioTransceiver> transceiver,
                  rtc::scoped_refptr<webrtc::AudioTrackInterface> track,
                  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
                  mrsLocalAudioTrackInteropHandle interop_handle) noexcept;

  ~LocalAudioTrack() override;

  /// Get the name of the local audio track.
  std::string GetName() const noexcept override { return track_name_; }

  /// Enable or disable the audio track. An enabled track streams its content
  /// from its source to the remote peer. A disabled audio track only sends
  /// black frames.
  void SetEnabled(bool enabled) const noexcept;

  /// Check if the track is enabled.
  /// See |SetEnabled(bool)|.
  [[nodiscard]] bool IsEnabled() const noexcept;

  [[nodiscard]] RefPtr<AudioTransceiver> GetTransceiver() const
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

  /// Internal callback on added to a peer connection to update the internal
  /// state of the object.
  void OnAddedToPeerConnection(
      PeerConnection& owner,
      RefPtr<AudioTransceiver> transceiver,
      rtc::scoped_refptr<webrtc::RtpSenderInterface> sender);

  /// Internal callback on removed from a peer connection to update the internal
  /// state of the object.
  void OnRemovedFromPeerConnection(
      PeerConnection& old_owner,
      RefPtr<AudioTransceiver> old_transceiver,
      rtc::scoped_refptr<webrtc::RtpSenderInterface> old_sender);

  void RemoveFromPeerConnection(webrtc::PeerConnectionInterface& peer);

 private:
  /// Underlying core implementation.
  rtc::scoped_refptr<webrtc::AudioTrackInterface> track_;

  /// RTP sender this track is associated with.
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender_;

  /// Transceiver this track is associated with, if any.
  RefPtr<AudioTransceiver> transceiver_;

  /// Optional interop handle, if associated with an interop wrapper.
  mrsLocalAudioTrackInteropHandle interop_handle_{};

  /// Cached track name, to avoid dispatching on signaling thread.
  const std::string track_name_;
};

}  // namespace Microsoft::MixedReality::WebRTC

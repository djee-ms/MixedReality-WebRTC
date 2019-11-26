// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "api/rtpsenderinterface.h"

#include "callback.h"
#include "interop/local_video_track_interop.h"
#include "local_video_track.h"
#include "str.h"
#include "tracked_object.h"
#include "video_frame_observer.h"

namespace Microsoft::MixedReality::WebRTC {
class PeerConnection;
}

namespace Microsoft::MixedReality::WebRTC::impl {

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
class LocalVideoTrackImpl : public LocalVideoTrack {
 public:
  LocalVideoTrackImpl(PeerConnection& owner,
                      rtc::scoped_refptr<webrtc::VideoTrackInterface> track,
                      rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
                      mrsLocalVideoTrackInteropHandle interop_handle) noexcept;
  MRS_API ~LocalVideoTrackImpl() override;

  /// Get the name of the local video track.
  MRS_API str GetName() const noexcept override;

  /// Enable or disable the video track. An enabled track streams its content
  /// from its source to the remote peer. A disabled video track only sends
  /// black frames.
  MRS_API void SetEnabled(bool enabled) const noexcept override;

  /// Check if the track is enabled.
  /// See |SetEnabled(bool)|.
  MRS_API [[nodiscard]] bool IsEnabled() const noexcept override;

  /// Register a callback to get notified on frame available,
  /// and received that frame as a I420-encoded buffer.
  /// This is not exclusive and can be used along another ARGB callback.
  MRS_API void SetCallback(I420AFrameReadyCallback callback) noexcept override {
      observer_.SetCallback(callback);
  }

  /// Register a callback to get notified on frame available,
  /// and received that frame as a raw decoded ARGB buffer.
  /// This is not exclusive and can be used along another I420 callback.
  MRS_API void SetCallback(ARGBFrameReadyCallback callback) noexcept override {
    observer_.SetCallback(callback);
  }

  //
  // Advanced use
  //

  [[nodiscard]] webrtc::VideoTrackInterface* impl() const;
  [[nodiscard]] webrtc::RtpSenderInterface* sender() const;

  [[nodiscard]] mrsLocalVideoTrackInteropHandle GetInteropHandle() const
      noexcept {
    return interop_handle_;
  }

  void RemoveFromPeerConnection(webrtc::PeerConnectionInterface& peer);

 private:
  /// Weak reference to the PeerConnection object owning this track.
  PeerConnection* owner_{};

  /// Underlying core implementation.
  rtc::scoped_refptr<webrtc::VideoTrackInterface> track_;

  /// RTP sender this track is associated with.
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender_;

  /// Optional interop handle, if associated with an interop wrapper.
  mrsLocalVideoTrackInteropHandle interop_handle_{};

  /// Video frame observer bridging the frame callbacks with WebRTC impl.
  VideoFrameObserver observer_;
};

}  // namespace Microsoft::MixedReality::WebRTC::impl

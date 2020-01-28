// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "callback.h"
#include "data_channel.h"
#include "mrs_errors.h"

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;
class LocalVideoTrack;
class ExternalVideoTrackSource;
class DataChannel;

struct BitrateSettings {
  std::optional<int> start_bitrate_bps;
  std::optional<int> min_bitrate_bps;
  std::optional<int> max_bitrate_bps;
};

struct IceCandidate {
  std::string sdp_mid;
  std::string candidate;
  int sdp_mline_index;
};

enum class SdpType { kOffer, kAnswer };

struct SdpDescription {
  SdpType type;
  std::string sdp;
};

/// The PeerConnection class is the entry point to most of WebRTC.
/// It encapsulates a single connection between a local peer and a remote peer,
/// and hosts some critical events for signaling and video rendering.
///
/// The high level flow to establish a connection is as follow:
/// - Create a peer connection object from a factory with
/// PeerConnection::create().
/// - Register a custom callback to the various signaling events.
/// - Optionally add audio/video/data tracks. These can also be added after the
/// connection is established, but see remark below.
/// - Create a peer connection offer, or wait for the remote peer to send an
/// offer, and respond with an answer.
///
/// At any point, before or after the connection is initated (CreateOffer() or
/// CreateAnswer()) or established (RegisterConnectedCallback()), some audio,
/// video, and data tracks can be added to it, with the following notable
/// remarks and restrictions:
/// - Data tracks use the DTLS/SCTP protocol and are encrypted; this requires a
/// handshake to exchange encryption secrets. This exchange is only performed
/// during the initial connection handshake if at least one data track is
/// present. As a consequence, at least one data track needs to be added before
/// calling CreateOffer() or CreateAnswer() if the application ever need to use
/// data channels. Otherwise trying to add a data channel after that initial
/// handshake will always fail.
/// - Adding and removing any kind of tracks after the connection has been
/// initiated result in a RenegotiationNeeded event to perform a new track
/// negotitation, which requires signaling to be working. Therefore it is
/// recommended when this is known in advance to add tracks before starting to
/// establish a connection, to perform the first handshake with the correct
/// tracks offer/answer right away.
class PeerConnection {
 public:
  /// Create a new PeerConnection based on the given |config|.
  /// This serves as the constructor for PeerConnection.
  static std::shared_ptr<PeerConnection> Create(
      const PeerConnectionConfiguration& config);

  //
  // Errors
  //

  virtual void CheckResult(mrsResult result) const { ThrowOnError(result); }

  //
  // Signaling
  //

  /// Callback fired when the state of the ICE connection changed.
  /// Note that the current implementation (m71) mixes the state of ICE and
  /// DTLS, so this does not correspond exactly to
  using IceStateChangedCallback = std::function<void(IceConnectionState)>;

  /// Register a custom IceStateChangedCallback.
  void RegisterIceStateChangedCallback(
      IceStateChangedCallback&& callback) noexcept;

  /// Callback fired when the state of the ICE gathering changed.
  using IceGatheringStateChangedCallback =
      std::function<void(IceGatheringState)>;

  /// Register a custom IceStateChangedCallback.
  void RegisterIceGatheringStateChangedCallback(
      IceGatheringStateChangedCallback&& callback) noexcept;

  /// Callback fired when some SDP negotiation needs to be initiated, often
  /// because some tracks have been added to or removed from the peer
  /// connection, to notify the remote peer of the change.
  /// Typically an implementation will call CreateOffer() when receiving this
  /// notification to initiate a new SDP exchange. Failing to do so will prevent
  /// the remote peer from being informed about track changes.
  using RenegotiationNeededCallback = std::function<void>;

  /// Register a custom RenegotiationNeededCallback.
  void RegisterRenegotiationNeededCallback(
      RenegotiationNeededCallback&& callback) noexcept;

  /// Notify the WebRTC engine that an ICE candidate has been received.
  void AddIceCandidate(const IceCandidate& candidate);

  /// Notify the WebRTC engine that an SDP offer message has been received.
  std::future<void> SetRemoteDescriptionAsync(const SdpDescription& desc);

  //
  // Connection
  //

  /// Callback fired when the peer connection is established.
  /// This guarantees that the handshake process has terminated successfully,
  /// but does not guarantee that ICE exchanges are done.
  using ConnectedCallback = Callback<>;

  /// Register a custom ConnectedCallback.
  void RegisterConnectedCallback(ConnectedCallback&& callback) noexcept;

  void SetBitrate(const BitrateSettings& settings);

  /// Create an SDP offer to attempt to establish a connection with the remote
  /// peer.
  std::future<SdpDescription> CreateOfferAsync();

  /// Create an SDP answer to accept a previously-received offer to establish a
  /// connection with the remote peer.
  std::future<SdpDescription> CreateAnswerAsync();

  /// Close the peer connection. After the connection is closed, it cannot be
  /// opened again with the same C++ object. Instantiate a new |PeerConnection|
  /// object instead to create a new connection. No-op if already closed.
  void Close();

  /// Check if the connection is closed. This returns |true| once |Close()| has
  /// been called.
  bool IsClosed() const noexcept { return (handle_ == nullptr); }

  //
  // Remote tracks
  //

  /// Callback fired when a remote track is added to the peer connection.
  using TrackAddedCallback = std::function<void(TrackKind)>;

  /// Register a custom TrackAddedCallback.
  void RegisterTrackAddedCallback(TrackAddedCallback&& callback);

  /// Callback fired when a remote track is removed from the peer connection.
  using TrackRemovedCallback = std::function<void(TrackKind)>;

  /// Register a custom TrackRemovedCallback.
  void RegisterTrackRemovedCallback(TrackRemovedCallback&& callback);

  //
  // Video
  //

  /// Register a custom callback invoked when a remote video frame has been
  /// received and decompressed, and is ready to be displayed locally.
  // void RegisterRemoteVideoFrameCallback(I420AFrameReadyCallback callback);

  /// Register a custom callback invoked when a remote video frame has been
  /// received and decompressed, and is ready to be displayed locally.
  // void RegisterRemoteVideoFrameCallback(Argb32FrameReadyCallback callback);

  /// Add a video track to the peer connection. If no RTP sender/transceiver
  /// exist, create a new one for that track.
  ErrorOr<std::shared_ptr<LocalVideoTrack>> AddLocalVideoTrack(
      std::string_view track_name,
      const VideoDeviceConfiguration& config);

  /// Remove a local video track from the peer connection.
  /// The underlying RTP sender/transceiver are kept alive but inactive.
  void RemoveLocalVideoTrack(LocalVideoTrack& video_track);

  /// Remove all tracks sharing the given video track source.
  /// Note that currently video source sharing is not supported, so this will
  /// remove at most a single track backed by the given source.
  void RemoveLocalVideoTracksFromSource(ExternalVideoTrackSource& source);

  /// Rounding mode of video frame height for |SetFrameHeightRoundMode()|.
  /// This is only used on HoloLens 1 (UWP x86).
  enum class FrameHeightRoundMode {
    /// Leave frames unchanged.
    kNone,

    /// Crop frame height to the nearest multiple of 16.
    /// ((height - nearestLowerMultipleOf16) / 2) rows are cropped from the top
    /// and (height - nearestLowerMultipleOf16 - croppedRowsTop) rows are
    /// cropped from the bottom.
    kCrop = 1,

    /// Pad frame height to the nearest multiple of 16.
    /// ((nearestHigherMultipleOf16 - height) / 2) rows are added symmetrically
    /// at the top and (nearestHigherMultipleOf16 - height - addedRowsTop) rows
    /// are added symmetrically at the bottom.
    kPad = 2
  };

  /// [HoloLens 1 only]
  /// Use this function to select whether resolutions where height is not
  /// multiple of 16 should be cropped, padded or left unchanged. Defaults to
  /// FrameHeightRoundMode::kCrop to avoid severe artifacts produced by the
  /// H.264 hardware encoder. The default value is applied when creating the
  /// first peer connection, so can be overridden after it.
  static void SetFrameHeightRoundMode(FrameHeightRoundMode value);

  //
  // Audio
  //

  /// Register a custom callback invoked when a local audio frame is ready to be
  /// output.
  ///
  /// FIXME - Current implementation of AddSink() for the local audio capture
  /// device is no-op. So this callback is never fired.
  // void RegisterLocalAudioFrameCallback(AudioFrameReadyCallback callback)
  // noexcept;

  /// Register a custom callback invoked when a remote audio frame has been
  /// received and uncompressed, and is ready to be output locally.
  // void RegisterRemoteAudioFrameCallback(AudioFrameReadyCallback callback)
  // noexcept;

  /// Add to the peer connection an audio track backed by a local audio capture
  /// device. If no RTP sender/transceiver exist, create a new one for that
  /// track.
  ///
  /// Note: currently a single local video track is supported per peer
  /// connection.
  void AddLocalAudioTrack();

  /// Remove the existing local audio track from the peer connection.
  /// The underlying RTP sender/transceiver are kept alive but inactive.
  ///
  /// Note: currently a single local audio track is supported per peer
  /// connection.
  void RemoveLocalAudioTrack();

  /// Enable or disable the local audio track. Disabled audio tracks are still
  /// active but are silent, and do not consume network bandwidth. Additionally,
  /// enabling/disabling the local audio track does not require an SDP exchange.
  /// Therefore this is a cheaper alternative to removing and re-adding the
  /// track.
  ///
  /// Note: currently a single local audio track is supported per peer
  /// connection.
  void SetLocalAudioTrackEnabled(bool enabled = true);

  /// Check if the local audio frame is enabled.
  ///
  /// Note: currently a single local audio track is supported per peer
  /// connection.
  bool IsLocalAudioTrackEnabled() const noexcept;

  //
  // Data channel
  //

  /// Callback invoked by the native layer when a new data channel is received
  /// from the remote peer and added locally.
  using DataChannelAddedCallback =
      std::function<void(const std::shared_ptr<DataChannel>&)>;

  /// Callback invoked by the native layer when a data channel is removed from
  /// the remote peer and removed locally.
  using DataChannelRemovedCallback =
      std::function<void(const std::shared_ptr<DataChannel>&)>;

  /// Register a custom callback invoked when a new data channel is received
  /// from the remote peer and added locally.
  void RegisterDataChannelAddedCallback(
      DataChannelAddedCallback callback) noexcept;

  /// Register a custom callback invoked when a data channel is removed by the
  /// remote peer and removed locally.
  void RegisterDataChannelRemovedCallback(
      DataChannelRemovedCallback callback) noexcept;

  /// Create a new data channel and add it to the peer connection.
  /// This invokes the DataChannelAdded callback.
  std::shared_ptr<DataChannel> AddDataChannel(int id,
                                              std::string_view label,
                                              bool ordered,
                                              bool reliable);

  /// Close and remove a given data channel.
  /// This invokes the DataChannelRemoved callback.
  void RemoveDataChannel(const DataChannel& data_channel);

  /// Close and remove all data channels at once.
  /// This invokes the DataChannelRemoved callback for each data channel.
  void RemoveAllDataChannels();

  /// Notification from a non-negotiated DataChannel that it is open, so that
  /// the PeerConnection can fire a DataChannelAdded event. This is called
  /// automatically by non-negotiated data channels; do not call manually.
  void OnDataChannelAdded(const DataChannel& data_channel);

  /// Internal use.
  // void GetStats(webrtc::RTCStatsCollectorCallback* callback);

  //
  // Advanced use
  //

  Result RegisterInteropCallbacks(
      const mrsPeerConnectionInteropCallbacks& callbacks) noexcept;

 private:
  PeerConnection(const PeerConnection&) = delete;
  PeerConnection& operator=(const PeerConnection&) = delete;

  PeerConnectionHandle handle_{};

  PeerConnectionHandle GetHandle() const {
    if (IsClosed()) {
      throw new ConnectionClosedException();
    }
    return handle_;
  }
};

}  // namespace Microsoft::MixedReality::WebRTC

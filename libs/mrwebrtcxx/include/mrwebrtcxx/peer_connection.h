// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <future>
#include <optional>

#include <mrwebrtcxx/audio_frame.h>
#include <mrwebrtcxx/callback.h>
#include <mrwebrtcxx/data_channel.h>
#include <mrwebrtcxx/exceptions.h>
#include <mrwebrtcxx/video_frame.h>

namespace Microsoft::MixedReality::WebRTC {

class DataChannel;
class ExternalVideoTrackSource;
class LocalAudioTrack;
class LocalVideoTrack;
class PeerConnection;
class RemoteAudioTrack;
class RemoteVideoTrack;

struct BitrateSettings {
  std::optional<int> start_bitrate_bps;
  std::optional<int> min_bitrate_bps;
  std::optional<int> max_bitrate_bps;
};

struct IceCandidate {
  std::string sdp_mid;
  std::string candidate;
  int sdp_mline_index{-1};
};

enum class SdpType : uint8_t { kUnknown, kOffer, kAnswer };

struct SdpDescription {
  std::string sdp;
  SdpType type{SdpType::kUnknown};
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
  static std::unique_ptr<PeerConnection> Create(
      const mrsPeerConnectionConfiguration& config);

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
  using IceStateChangedCallback = Callback<mrsIceConnectionState>;

  /// Register a custom IceStateChangedCallback.
  void RegisterIceStateChangedCallback(
      IceStateChangedCallback&& callback) noexcept {
    ice_state_changed_cb_ = std::move(callback);
    mrsPeerConnectionRegisterIceStateChangedCallback(
        GetHandle(), &IceStateChangedCallback::StaticExec,
        ice_state_changed_cb_.GetUserData());
  }

  /// Callback fired when the state of the ICE gathering changed.
  using IceGatheringStateChangedCallback = Callback<mrsIceGatheringState>;

  /// Register a custom IceStateChangedCallback.
  void RegisterIceGatheringStateChangedCallback(
      IceGatheringStateChangedCallback&& callback) noexcept {
    ice_gathering_state_changed_cb_ = std::move(callback);
    mrsPeerConnectionRegisterIceGatheringStateChangedCallback(
        GetHandle(), &IceGatheringStateChangedCallback::StaticExec,
        ice_gathering_state_changed_cb_.GetUserData());
  }

  /// Callback fired when some SDP negotiation needs to be initiated, often
  /// because some tracks have been added to or removed from the peer
  /// connection, to notify the remote peer of the change.
  /// Typically an implementation will call CreateOffer() when receiving this
  /// notification to initiate a new SDP exchange. Failing to do so will prevent
  /// the remote peer from being informed about track changes.
  using RenegotiationNeededCallback = Callback<void>;

  /// Register a custom RenegotiationNeededCallback.
  void RegisterRenegotiationNeededCallback(
      RenegotiationNeededCallback&& callback) noexcept {
    renegotiation_needed_cb_ = std::move(callback);
    mrsPeerConnectionRegisterRenegotiationNeededCallback(
        GetHandle(), &RenegotiationNeededCallback::StaticExec,
        renegotiation_needed_cb_.GetUserData());
  }

  /// Notify the WebRTC engine that an ICE candidate has been received.
  void AddIceCandidate(const IceCandidate& candidate);

  /// Notify the WebRTC engine that an SDP offer or answer message has been
  /// received, and apply it to the local peer. This updates the peer by
  /// creating transceivers and remote tracks if necessary.
  ///
  /// When applying an offer message, this returns a future whose promise must
  /// successfully complete before |CreateAnswerAsync()| can be called.
  ///
  /// When applying an answer message, the completion of the returned future's
  /// promise marks the end of the SDP exchange.
  std::future<void> SetRemoteDescriptionAsync(const SdpDescription& desc);

  //
  // Connection
  //

  /// Callback fired when the peer connection is established.
  /// This guarantees that the handshake process has terminated successfully,
  /// but does not guarantee that ICE exchanges are done.
  using ConnectedCallback = Callback<void>;

  /// Register a custom ConnectedCallback.
  void RegisterConnectedCallback(ConnectedCallback&& callback) noexcept {
    connected_cb_ = std::move(callback);
    mrsPeerConnectionRegisterConnectedCallback(GetHandle(),
                                               &ConnectedCallback::StaticExec,
                                               connected_cb_.GetUserData());
  }

  void SetBitrate(const BitrateSettings& settings);

  /// Create an SDP offer to attempt to establish a connection with the remote
  /// peer. When the SDP offer description is ready, it should be sent by the
  /// user to the remote peer via the chosen signaling solution.
  std::future<SdpDescription> CreateOfferAsync();

  /// Create an SDP answer to accept a previously-received offer to establish a
  /// connection with the remote peer. When the SDP answer description is ready,
  /// it should be sent by the user to the remote peer via the chosen signaling
  /// solution.
  std::future<SdpDescription> CreateAnswerAsync();

  /// Close the peer connection. After the connection is closed, it cannot be
  /// opened again with the same C++ object. Instantiate a new |PeerConnection|
  /// object instead to create a new connection. No-op if already closed.
  void Close();

  /// Check if the connection is closed. This returns |true| once |Close()| has
  /// been called.
  [[nodiscard]] bool IsClosed() const noexcept { return (handle_ == nullptr); }

  //
  // Remote tracks
  //

  //
  // Video
  //

  /// Callback fired when a remote video track is added to the peer connection.
  using VideoTrackAddedCallback = Callback<RemoteVideoTrack>;

  /// Register a custom VideoTrackAddedCallback.
  void RegisterVideoTrackAddedCallback(
      VideoTrackAddedCallback&& callback) noexcept {
    video_track_added_cb_ = std::move(callback);
    mrsPeerConnectionRegisterVideoTrackAddedCallback(
        GetHandle(), &VideoTrackAddedCallback::StaticExec,
        video_track_added_cb_.GetUserData());
  }

  /// Callback fired when a remote video track is removed from the peer
  /// connection.
  using VideoTrackRemovedCallback = Callback<RemoteVideoTrack>;

  /// Register a custom VideoTrackRemovedCallback.
  void RegisterVideoTrackRemovedCallback(
      VideoTrackRemovedCallback&& callback) noexcept {
    video_track_removed_cb_ = std::move(callback);
    mrsPeerConnectionRegisterVideoTrackRemovedCallback(
        GetHandle(), &VideoTrackRemovedCallback::StaticExec,
        video_track_removed_cb_.GetUserData());
  }

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

  /// Callback fired when a remote audio track is added to the peer connection.
  using AudioTrackAddedCallback = Callback<RemoteAudioTrack>;

  /// Register a custom AudioTrackAddedCallback.
  void RegisterAudioTrackAddedCallback(
      AudioTrackAddedCallback&& callback) noexcept {
    audio_track_added_cb_ = std::move(callback);
    mrsPeerConnectionRegisterAudioTrackAddedCallback(
        GetHandle(), &AudioTrackAddedCallback::StaticExec,
        audio_track_added_cb_.GetUserData());
  }

  /// Callback fired when a remote audio track is removed from the peer
  /// connection.
  using AudioTrackRemovedCallback = Callback<RemoteAudioTrack>;

  /// Register a custom AudioTrackRemovedCallback.
  void RegisterAudioTrackRemovedCallback(
      AudioTrackRemovedCallback&& callback) noexcept {
    audio_track_removed_cb_ = std::move(callback);
    mrsPeerConnectionRegisterAudioTrackRemovedCallback(
        GetHandle(), &AudioTrackRemovedCallback::StaticExec,
        audio_track_removed_cb_.GetUserData());
  }

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
  void RemoveDataChannel(DataChannel& data_channel);

 private:
  PeerConnection(mrsPeerConnectionHandle handle) noexcept : handle_(handle) {}
  PeerConnection(const PeerConnection&) = delete;
  PeerConnection& operator=(const PeerConnection&) = delete;

  mrsPeerConnectionHandle GetHandle() const {
    if (IsClosed()) {
      throw new ConnectionClosedException();
    }
    return handle_;
  }

  mrsPeerConnectionHandle handle_{};
  IceStateChangedCallback ice_state_changed_cb_;
  IceGatheringStateChangedCallback ice_gathering_state_changed_cb_;
  RenegotiationNeededCallback renegotiation_needed_cb_;
  ConnectedCallback connected_cb_;
  AudioTrackAddedCallback audio_track_added_cb_;
  VideoTrackAddedCallback video_track_added_cb_;
  AudioTrackRemovedCallback audio_track_removed_cb_;
  VideoTrackRemovedCallback video_track_removed_cb_;
};

}  // namespace Microsoft::MixedReality::WebRTC

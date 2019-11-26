// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <optional>
#include <string_view>

#include "audio_frame.h"
#include "callback.h"
#include "data_channel.h"
#include "mrs_errors.h"
#include "refptr.h"
#include "tracked_object.h"
#include "video_frame.h"

struct mrsPeerConnectionInteropCallbacks;

using mrsPeerConnectionInteropHandle = void*;
using mrsDataChannelInteropHandle = void*;

using DataChannelHandle = void*;

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;
class LocalVideoTrack;
class DataChannel;

struct BitrateSettings {
  std::optional<int> start_bitrate_bps;
  std::optional<int> min_bitrate_bps;
  std::optional<int> max_bitrate_bps;
};

/// ICE transport type. See webrtc::PeerConnectionInterface::IceTransportsType.
/// Currently values are aligned, but kept as a separate structure to allow
/// backward compatilibity in case of changes in WebRTC.
enum class IceTransportType : int32_t {
  kNone = 0,
  kRelay = 1,
  kNoHost = 2,
  kAll = 3
};

/// Bundle policy. See webrtc::PeerConnectionInterface::BundlePolicy.
/// Currently values are aligned, but kept as a separate structure to allow
/// backward compatilibity in case of changes in WebRTC.
enum class BundlePolicy : int32_t {
  kBalanced = 0,
  kMaxBundle = 1,
  kMaxCompat = 2
};

/// SDP semantic (protocol dialect) for (re)negotiating a peer connection.
/// This cannot be changed after the connection is established.
enum class SdpSemantic : int32_t {
  /// Unified Plan - default and recommended. Standardized in WebRTC 1.0.
  kUnifiedPlan = 0,
  /// Plan B - deprecated and soon to be removed. Do not use unless for
  /// compability with an older implementation. This is non-standard.
  kPlanB = 1
};

/// Single STUN/TURN server for Interactive Connectivity Establishment (ICE).
/// The ICE protocol enables NAT traversal.
struct IceServer {
  /// List of URLs for the server, often a single one.
  std::vector<str> urls;

  /// Optional username for non-anonymous connection.
  str username;

  /// Optional password for non-anonymous connection.
  str password;
};

/// Configuration to intialize a peer connection object.
class PeerConnectionConfiguration final {
 public:
  /// ICE transport type for the connection.
  IceTransportType ice_transport_type = IceTransportType::kAll;

  /// Bundle policy for the connection.
  BundlePolicy bundle_policy = BundlePolicy::kBalanced;

  /// SDP semantic for connection negotiation.
  /// Do not use Plan B unless there is a problem with Unified Plan.
  SdpSemantic sdp_semantic = SdpSemantic::kUnifiedPlan;

  MRS_API PeerConnectionConfiguration();
  MRS_API ~PeerConnectionConfiguration();
  MRS_API void AddIceServer(const IceServer& server);
  MRS_API void AddIceServer(IceServer&& server);
  MRS_API void AddIceServersFromEncodedString(const str& encoded);

 private:
  /// List of ICE servers for NAT traversal. This can be empty, in which case
  /// only direct connections will be possible.
  std::vector<IceServer> ice_servers;
};

/// State of the ICE connection.
/// See https://www.w3.org/TR/webrtc/#rtciceconnectionstate-enum.
/// Note that there is a mismatch currently due to the m71 implementation.
enum IceConnectionState : int32_t {
  kNew = 0,
  kChecking = 1,
  kConnected = 2,
  kCompleted = 3,
  kFailed = 4,
  kDisconnected = 5,
  kClosed = 6,
};

/// Kind of media track. Equivalent to
/// webrtc::MediaStreamTrackInterface::kind().
enum class TrackKind : uint32_t {
  kUnknownTrack = 0,
  kAudioTrack = 1,
  kVideoTrack = 2,
  kDataTrack = 3,
};

/// Kind of video profile. Equivalent to org::webRtc::VideoProfileKind.
enum class VideoProfileKind : int32_t {
  kUnspecified,
  kVideoRecording,
  kHighQualityPhoto,
  kBalancedVideoAndPhoto,
  kVideoConferencing,
  kPhotoSequence,
  kHighFrameRate,
  kVariablePhotoSequence,
  kHdrWithWcgVideo,
  kHdrWithWcgPhoto,
  kVideoHdr8,
};

/// Configuration for opening a local video capture device.
struct VideoDeviceConfiguration {
  /// Unique identifier of the video capture device to select, as returned by
  /// |mrsEnumVideoCaptureDevicesAsync|, or a null or empty string to select the
  /// default device.
  std::string_view video_device_id;

  /// Optional name of a video profile, if the platform supports it, or null to
  /// no use video profiles.
  std::string_view video_profile_id;

  /// Optional kind of video profile to select, if the platform supports it.
  /// If a video profile ID is specified with |video_profile_id| it is
  /// recommended to leave this as kUnspecified to avoid over-constraining the
  /// video capture format selection.
  VideoProfileKind video_profile_kind = VideoProfileKind::kUnspecified;

  /// Optional preferred capture resolution width, in pixels, or zero for
  /// unconstrained.
  std::optional<uint32_t> width;

  /// Optional preferred capture resolution height, in pixels, or zero for
  /// unconstrained.
  std::optional<uint32_t> height;

  /// Optional preferred capture framerate, in frame per second (FPS), or zero
  /// for unconstrained.
  /// This framerate is compared exactly to the one reported by the video
  /// capture device (webcam), so should be queried rather than hard-coded to
  /// avoid mismatches with video formats reporting e.g. 29.99 instead of 30.0.
  std::optional<double> framerate;

  /// On platforms supporting Mixed Reality Capture (MRC) like HoloLens, enable
  /// this feature. This produces a video track where the holograms rendering is
  /// overlaid over the webcam frame. This parameter is ignored on platforms not
  /// supporting MRC.
  /// Note that MRC is only available in exclusive-mode applications, or in
  /// shared apps with the restricted capability "rescap:screenDuplication". In
  /// any other case the capability will not be granted and MRC will silently
  /// fail, falling back to a simple webcam video feed without holograms.
  bool enable_mrc{true};

  /// When Mixed Reality Capture is enabled, enable or disable the recording
  /// indicator shown on screen.
  bool enable_mrc_recording_indicator{true};
};

/// Configuration for opening a local audio capture device.
struct AudioDeviceConfiguration {
  std::optional<bool> echo_cancellation;
  std::optional<bool> auto_gain_control;
  std::optional<bool> noise_suppression;
  std::optional<bool> highpass_filter;
  std::optional<bool> typing_detection;
  std::optional<bool> aecm_generate_comfort_noise;
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
class PeerConnection : public TrackedObject {
 public:
  /// Create a new PeerConnection based on the given |config|.
  /// This serves as the constructor for PeerConnection.
  static MRS_API ErrorOr<RefPtr<PeerConnection>> MRS_CALL
  create(const PeerConnectionConfiguration& config,
         mrsPeerConnectionInteropHandle interop_handle);

  /// Set the name of the peer connection.
  MRS_API virtual void SetName(std::string_view name) = 0;

  //
  // Signaling
  //

  /// Callback fired when a local SDP message is ready to be sent to the remote
  /// peer by the signalling solution. The callback parameters are:
  /// - The null-terminated type of the SDP message. Valid values are "offer",
  /// "answer", and "ice".
  /// - The null-terminated SDP message content.
  using LocalSdpReadytoSendCallback = Callback<const char*, const char*>;

  /// Register a custom LocalSdpReadytoSendCallback.
  virtual void RegisterLocalSdpReadytoSendCallback(
      LocalSdpReadytoSendCallback&& callback) noexcept = 0;

  /// Callback fired when a local ICE candidate message is ready to be sent to
  /// the remote peer by the signalling solution. The callback parameters are:
  /// - The null-terminated ICE message content.
  /// - The mline index.
  /// - The MID string value.
  using IceCandidateReadytoSendCallback =
      Callback<const char*, int, const char*>;

  /// Register a custom IceCandidateReadytoSendCallback.
  virtual void RegisterIceCandidateReadytoSendCallback(
      IceCandidateReadytoSendCallback&& callback) noexcept = 0;

  /// Callback fired when the state of the ICE connection changed.
  /// Note that the current implementation (m71) mixes the state of ICE and
  /// DTLS, so this does not correspond exactly to
  using IceStateChangedCallback = Callback<IceConnectionState>;

  /// Register a custom IceStateChangedCallback.
  virtual void RegisterIceStateChangedCallback(
      IceStateChangedCallback&& callback) noexcept = 0;

  /// Callback fired when some SDP negotiation needs to be initiated, often
  /// because some tracks have been added to or removed from the peer
  /// connection, to notify the remote peer of the change.
  /// Typically an implementation will call CreateOffer() when receiving this
  /// notification to initiate a new SDP exchange. Failing to do so will prevent
  /// the remote peer from being informed about track changes.
  using RenegotiationNeededCallback = Callback<>;

  /// Register a custom RenegotiationNeededCallback.
  virtual void RegisterRenegotiationNeededCallback(
      RenegotiationNeededCallback&& callback) noexcept = 0;

  /// Notify the WebRTC engine that an ICE candidate has been received.
  virtual bool MRS_API AddIceCandidate(const char* sdp_mid,
                                       const int sdp_mline_index,
                                       const char* candidate) noexcept = 0;

  /// Notify the WebRTC engine that an SDP offer message has been received.
  virtual bool MRS_API SetRemoteDescription(const char* type,
                                            const char* sdp) noexcept = 0;

  //
  // Connection
  //

  /// Callback fired when the peer connection is established.
  /// This guarantees that the handshake process has terminated successfully,
  /// but does not guarantee that ICE exchanges are done.
  using ConnectedCallback = Callback<>;

  /// Register a custom ConnectedCallback.
  virtual void RegisterConnectedCallback(
      ConnectedCallback&& callback) noexcept = 0;

  virtual Result SetBitrate(const BitrateSettings& settings) noexcept = 0;

  /// Create an SDP offer to attempt to establish a connection with the remote
  /// peer. Once the offer message is ready, the LocalSdpReadytoSendCallback
  /// callback is invoked to deliver the message.
  virtual bool MRS_API CreateOffer() noexcept = 0;

  /// Create an SDP answer to accept a previously-received offer to establish a
  /// connection wit the remote peer. Once the answer message is ready, the
  /// LocalSdpReadytoSendCallback callback is invoked to deliver the message.
  virtual bool MRS_API CreateAnswer() noexcept = 0;

  /// Close the peer connection. After the connection is closed, it cannot be
  /// opened again with the same C++ object. Instantiate a new |PeerConnection|
  /// object instead to create a new connection. No-op if already closed.
  virtual void MRS_API Close() noexcept = 0;

  /// Check if the connection is closed. This returns |true| once |Close()| has
  /// been called.
  virtual bool MRS_API IsClosed() const noexcept = 0;

  //
  // Remote tracks
  //

  /// Callback fired when a remote track is added to the peer connection.
  using TrackAddedCallback = Callback<TrackKind>;

  /// Register a custom TrackAddedCallback.
  virtual void RegisterTrackAddedCallback(
      TrackAddedCallback&& callback) noexcept = 0;

  /// Callback fired when a remote track is removed from the peer connection.
  using TrackRemovedCallback = Callback<TrackKind>;

  /// Register a custom TrackRemovedCallback.
  virtual void RegisterTrackRemovedCallback(
      TrackRemovedCallback&& callback) noexcept = 0;

  //
  // Video
  //

  /// Register a custom callback invoked when a remote video frame has been
  /// received and decompressed, and is ready to be displayed locally.
  virtual void RegisterRemoteVideoFrameCallback(
      I420AFrameReadyCallback callback) noexcept = 0;

  /// Register a custom callback invoked when a remote video frame has been
  /// received and decompressed, and is ready to be displayed locally.
  virtual void RegisterRemoteVideoFrameCallback(
      ARGBFrameReadyCallback callback) noexcept = 0;

  /// Add to the peer connection a video track backed by a local video capture
  /// device (webcam). If no RTP sender/transceiver exist, create a new one for
  /// that track.
  virtual ErrorOr<RefPtr<LocalVideoTrack>> MRS_API
  AddLocalVideoTrack(std::string_view track_name,
                     const VideoDeviceConfiguration& config) noexcept = 0;

  /// Remove a local video track from the peer connection.
  /// The underlying RTP sender/transceiver are kept alive but inactive.
  virtual Error MRS_API
  RemoveLocalVideoTrack(LocalVideoTrack& video_track) noexcept = 0;

  //
  // Audio
  //

  /// Register a custom callback invoked when a local audio frame is ready to be
  /// output.
  ///
  /// FIXME - Current implementation of AddSink() for the local audio capture
  /// device is no-op. So this callback is never fired.
  virtual void RegisterLocalAudioFrameCallback(
      AudioFrameReadyCallback callback) noexcept = 0;

  /// Register a custom callback invoked when a remote audio frame has been
  /// received and uncompressed, and is ready to be output locally.
  virtual void RegisterRemoteAudioFrameCallback(
      AudioFrameReadyCallback callback) noexcept = 0;

  /// Add to the peer connection an audio track backed by a local audio capture
  /// device. If no RTP sender/transceiver exist, create a new one for that
  /// track.
  ///
  /// Note: currently a single local video track is supported per peer
  /// connection.
  virtual Result MRS_API
  AddLocalAudioTrack(std::string_view track_name,
                     const AudioDeviceConfiguration& config) noexcept = 0;

  /// Remove the existing local audio track from the peer connection.
  /// The underlying RTP sender/transceiver are kept alive but inactive.
  ///
  /// Note: currently a single local audio track is supported per peer
  /// connection.
  virtual void MRS_API RemoveLocalAudioTrack() noexcept = 0;

  /// Enable or disable the local audio track. Disabled audio tracks are still
  /// active but are silent, and do not consume network bandwidth. Additionally,
  /// enabling/disabling the local audio track does not require an SDP exchange.
  /// Therefore this is a cheaper alternative to removing and re-adding the
  /// track.
  ///
  /// Note: currently a single local audio track is supported per peer
  /// connection.
  virtual void MRS_API
  SetLocalAudioTrackEnabled(bool enabled = true) noexcept = 0;

  /// Check if the local audio frame is enabled.
  ///
  /// Note: currently a single local audio track is supported per peer
  /// connection.
  virtual bool MRS_API IsLocalAudioTrackEnabled() const noexcept = 0;

  //
  // Data channel
  //

  /// Callback invoked by the native layer when a new data channel is received
  /// from the remote peer and added locally.
  using DataChannelAddedCallback =
      Callback<mrsDataChannelInteropHandle, DataChannelHandle>;

  /// Callback invoked by the native layer when a data channel is removed from
  /// the remote peer and removed locally.
  using DataChannelRemovedCallback =
      Callback<mrsDataChannelInteropHandle, DataChannelHandle>;

  /// Register a custom callback invoked when a new data channel is received
  /// from the remote peer and added locally.
  virtual void RegisterDataChannelAddedCallback(
      DataChannelAddedCallback callback) noexcept = 0;

  /// Register a custom callback invoked when a data channel is removed by the
  /// remote peer and removed locally.
  virtual void RegisterDataChannelRemovedCallback(
      DataChannelRemovedCallback callback) noexcept = 0;

  /// Create a new data channel and add it to the peer connection.
  /// This invokes the DataChannelAdded callback.
  ErrorOr<RefPtr<DataChannel>> MRS_API virtual AddDataChannel(
      int id,
      std::string_view label,
      bool ordered,
      bool reliable,
      mrsDataChannelInteropHandle dataChannelInteropHandle) noexcept = 0;

  /// Close and remove a given data channel.
  /// This invokes the DataChannelRemoved callback.
  virtual void MRS_API
  RemoveDataChannel(DataChannel& data_channel) noexcept = 0;

  /// Close and remove all data channels at once.
  /// This invokes the DataChannelRemoved callback for each data channel.
  virtual void MRS_API RemoveAllDataChannels() noexcept = 0;

  /// Notification from a non-negotiated DataChannel that it is open, so that
  /// the PeerConnection can fire a DataChannelAdded event. This is called
  /// automatically by non-negotiated data channels; do not call manually.
  virtual void MRS_API
  OnDataChannelAdded(const DataChannel& data_channel) noexcept = 0;

  //
  // Advanced use
  //

  virtual Result RegisterInteropCallbacks(
      const mrsPeerConnectionInteropCallbacks& callbacks) noexcept = 0;
};

}  // namespace Microsoft::MixedReality::WebRTC

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// This is a precompiled header, it must be on its own, followed by a blank
// line, to prevent clang-format from reordering it with other headers.
#include "pch.h"

#include "audio_frame_observer.h"
#include "data_channel.h"
#include "local_video_track.h"
#include "media/local_audio_track.h"
#include "media/remote_audio_track.h"
#include "media/remote_video_track.h"
#include "peer_connection.h"
#include "sdp_utils.h"
#include "video_frame_observer.h"

// Internal
#include "interop/global_factory.h"
#include "interop/interop_api.h"

#include <functional>

#if defined(_M_IX86) /* x86 */ && defined(WINAPI_FAMILY) && \
    (WINAPI_FAMILY == WINAPI_FAMILY_APP) /* UWP app */ &&   \
    defined(_WIN32_WINNT_WIN10) &&                          \
    _WIN32_WINNT >= _WIN32_WINNT_WIN10 /* Win10 */

// Defined in
// external/webrtc-uwp-sdk/webrtc/xplatform/webrtc/third_party/winuwp_h264/H264Encoder/H264Encoder.cc
static constexpr int kFrameHeightCrop = 1;
extern int webrtc__WinUWPH264EncoderImpl__frame_height_round_mode;

// Stop WinRT from polluting the global namespace
// https://developercommunity.visualstudio.com/content/problem/859178/asyncinfoh-defines-the-error-symbol-at-global-name.html
#define _HIDE_GLOBAL_ASYNC_STATUS 1

#include <Windows.Foundation.h>
#include <windows.graphics.holographic.h>
#include <wrl\client.h>
#include <wrl\wrappers\corewrappers.h>

namespace {

bool CheckIfHololens() {
  // The best way to check if we are running on Hololens is checking if this is
  // a x86 Windows device with a transparent holographic display (AR).

  using namespace Microsoft::WRL;
  using namespace Microsoft::WRL::Wrappers;
  using namespace ABI::Windows::Foundation;
  using namespace ABI::Windows::Graphics::Holographic;

#define RETURN_IF_ERROR(...) \
  if (FAILED(__VA_ARGS__)) { \
    return false;            \
  }

  RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);

  // HolographicSpace.IsAvailable
  ComPtr<IHolographicSpaceStatics2> holo_space_statics;
  RETURN_IF_ERROR(GetActivationFactory(
      HStringReference(
          RuntimeClass_Windows_Graphics_Holographic_HolographicSpace)
          .Get(),
      &holo_space_statics));
  boolean is_holo_space_available;
  RETURN_IF_ERROR(
      holo_space_statics->get_IsAvailable(&is_holo_space_available));
  if (!is_holo_space_available) {
    // Not a holographic device.
    return false;
  }

  // HolographicDisplay.GetDefault().IsOpaque
  ComPtr<IHolographicDisplayStatics> holo_display_statics;
  RETURN_IF_ERROR(GetActivationFactory(
      HStringReference(
          RuntimeClass_Windows_Graphics_Holographic_HolographicDisplay)
          .Get(),
      &holo_display_statics));
  ComPtr<IHolographicDisplay> holo_display;
  RETURN_IF_ERROR(holo_display_statics->GetDefault(&holo_display));
  boolean is_opaque;
  RETURN_IF_ERROR(holo_display->get_IsOpaque(&is_opaque));
  // Hololens if not opaque (otherwise VR).
  return !is_opaque;
#undef RETURN_IF_ERROR
}

bool IsHololens() {
  static bool is_hololens = CheckIfHololens();
  return is_hololens;
}

}  // namespace

namespace Microsoft::MixedReality::WebRTC {
void PeerConnection::SetFrameHeightRoundMode(FrameHeightRoundMode value) {
  if (IsHololens()) {
    webrtc__WinUWPH264EncoderImpl__frame_height_round_mode = (int)value;
  }
}
}  // namespace Microsoft::MixedReality::WebRTC

#else

namespace Microsoft::MixedReality::WebRTC {
void PeerConnection::SetFrameHeightRoundMode(FrameHeightRoundMode /*value*/) {}

}  // namespace Microsoft::MixedReality::WebRTC

#endif

namespace {

using namespace Microsoft::MixedReality::WebRTC;

Result ResultFromRTCErrorType(webrtc::RTCErrorType type) {
  using namespace webrtc;
  switch (type) {
    case RTCErrorType::NONE:
      return Result::kSuccess;
    case RTCErrorType::UNSUPPORTED_OPERATION:
    case RTCErrorType::UNSUPPORTED_PARAMETER:
      return Result::kUnsupported;
    case RTCErrorType::INVALID_PARAMETER:
    case RTCErrorType::INVALID_RANGE:
      return Result::kInvalidParameter;
    case RTCErrorType::INVALID_STATE:
      return Result::kNotInitialized;
    default:
      return Result::kUnknownError;
  }
}

Microsoft::MixedReality::WebRTC::Error ErrorFromRTCError(
    const webrtc::RTCError& error) {
  return Microsoft::MixedReality::WebRTC::Error(
      ResultFromRTCErrorType(error.type()), error.message());
}

Microsoft::MixedReality::WebRTC::Error ErrorFromRTCError(
    webrtc::RTCError&& error) {
  // Ideally would move the std::string out of |error|, but doesn't look
  // possible at the moment.
  return Microsoft::MixedReality::WebRTC::Error(
      ResultFromRTCErrorType(error.type()), error.message());
}

class PeerConnectionImpl;

class StreamObserver : public webrtc::ObserverInterface {
 public:
  StreamObserver(PeerConnectionImpl& owner,
                 rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
      : owner_(owner), stream_(std::move(stream)) {}

 protected:
  PeerConnectionImpl& owner_;
  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream_;

  //
  // ObserverInterface
  //

  void OnChanged() override;
};

/// Implementation of PeerConnection, which also implements
/// PeerConnectionObserver at the same time to simplify interaction with
/// the underlying implementation object.
class PeerConnectionImpl : public PeerConnection,
                           public webrtc::PeerConnectionObserver {
 public:
  mrsPeerConnectionInteropHandle hhh;

  PeerConnectionImpl(mrsPeerConnectionInteropHandle interop_handle)
      : interop_handle_(interop_handle) {
    GlobalFactory::InstancePtr()->AddObject(ObjectType::kPeerConnection, this);
  }

  ~PeerConnectionImpl() noexcept {
    Close();
    GlobalFactory::InstancePtr()->RemoveObject(ObjectType::kPeerConnection,
                                               this);
  }

  void SetPeerImpl(rtc::scoped_refptr<webrtc::PeerConnectionInterface> impl) {
    peer_ = std::move(impl);
  }

  void SetName(std::string_view name) { name_ = name; }

  std::string GetName() const override { return name_; }

  void RegisterLocalSdpReadytoSendCallback(
      LocalSdpReadytoSendCallback&& callback) noexcept override {
    auto lock = std::scoped_lock{local_sdp_ready_to_send_callback_mutex_};
    local_sdp_ready_to_send_callback_ = std::move(callback);
  }

  void RegisterIceCandidateReadytoSendCallback(
      IceCandidateReadytoSendCallback&& callback) noexcept override {
    auto lock = std::scoped_lock{ice_candidate_ready_to_send_callback_mutex_};
    ice_candidate_ready_to_send_callback_ = std::move(callback);
  }

  void RegisterIceStateChangedCallback(
      IceStateChangedCallback&& callback) noexcept override {
    auto lock = std::scoped_lock{ice_state_changed_callback_mutex_};
    ice_state_changed_callback_ = std::move(callback);
  }

  void RegisterIceGatheringStateChangedCallback(
      IceGatheringStateChangedCallback&& callback) noexcept override {
    auto lock = std::scoped_lock{ice_gathering_state_changed_callback_mutex_};
    ice_gathering_state_changed_callback_ = std::move(callback);
  }

  void RegisterRenegotiationNeededCallback(
      RenegotiationNeededCallback&& callback) noexcept override {
    auto lock = std::scoped_lock{renegotiation_needed_callback_mutex_};
    renegotiation_needed_callback_ = std::move(callback);
  }

  bool AddIceCandidate(const char* sdp_mid,
                       const int sdp_mline_index,
                       const char* candidate) noexcept override;
  bool SetRemoteDescriptionAsync(const char* type,
                                 const char* sdp,
                                 Callback<> callback) noexcept override;

  void RegisterConnectedCallback(
      ConnectedCallback&& callback) noexcept override {
    auto lock = std::scoped_lock{connected_callback_mutex_};
    connected_callback_ = std::move(callback);
  }

  mrsResult SetBitrate(const BitrateSettings& settings) noexcept override {
    webrtc::BitrateSettings bitrate;
    bitrate.start_bitrate_bps = settings.start_bitrate_bps;
    bitrate.min_bitrate_bps = settings.min_bitrate_bps;
    bitrate.max_bitrate_bps = settings.max_bitrate_bps;
    return ResultFromRTCErrorType(peer_->SetBitrate(bitrate).type());
  }

  bool CreateOffer() noexcept override;
  bool CreateAnswer() noexcept override;
  void Close() noexcept override;
  bool IsClosed() const noexcept override;

  ErrorOr<RefPtr<VideoTransceiver>> AddVideoTransceiver(
      const VideoTransceiverInitConfig& config) noexcept override;
  ErrorOr<RefPtr<LocalVideoTrack>> AddLocalVideoTrack(
      rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track,
      mrsVideoTransceiverInteropHandle transceiver_interop_handle,
      mrsLocalVideoTrackInteropHandle track_interop_handle) noexcept override;
  Result RemoveLocalVideoTrack(LocalVideoTrack& video_track) noexcept override;
  void RemoveLocalVideoTracksFromSource(
      ExternalVideoTrackSource& source) noexcept override;

  void RegisterVideoTrackAddedCallback(
      VideoTrackAddedCallback&& callback) noexcept override {
    auto lock = std::scoped_lock{media_track_callback_mutex_};
    video_track_added_callback_ = std::move(callback);
  }

  void RegisterVideoTrackRemovedCallback(
      VideoTrackRemovedCallback&& callback) noexcept override {
    auto lock = std::scoped_lock{media_track_callback_mutex_};
    video_track_removed_callback_ = std::move(callback);
  }

  ErrorOr<RefPtr<AudioTransceiver>> AddAudioTransceiver(
      const AudioTransceiverInitConfig& config) noexcept override;
  ErrorOr<RefPtr<LocalAudioTrack>> AddLocalAudioTrack(
      rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track,
      mrsAudioTransceiverInteropHandle transceiver_interop_handle,
      mrsLocalAudioTrackInteropHandle track_interop_handle) noexcept override;
  Result RemoveLocalAudioTrack(LocalAudioTrack& audio_track) noexcept override;

  void RegisterAudioTrackAddedCallback(
      AudioTrackAddedCallback&& callback) noexcept override {
    auto lock = std::scoped_lock{media_track_callback_mutex_};
    audio_track_added_callback_ = std::move(callback);
  }

  void RegisterAudioTrackRemovedCallback(
      AudioTrackRemovedCallback&& callback) noexcept override {
    auto lock = std::scoped_lock{media_track_callback_mutex_};
    audio_track_removed_callback_ = std::move(callback);
  }

  void RegisterDataChannelAddedCallback(
      DataChannelAddedCallback callback) noexcept override {
    auto lock = std::scoped_lock{data_channel_added_callback_mutex_};
    data_channel_added_callback_ = std::move(callback);
  }

  void RegisterDataChannelRemovedCallback(
      DataChannelRemovedCallback callback) noexcept override {
    auto lock = std::scoped_lock{data_channel_removed_callback_mutex_};
    data_channel_removed_callback_ = std::move(callback);
  }

  ErrorOr<std::shared_ptr<DataChannel>> AddDataChannel(
      int id,
      std::string_view label,
      bool ordered,
      bool reliable,
      mrsDataChannelInteropHandle dataChannelInteropHandle) noexcept override;
  void RemoveDataChannel(const DataChannel& data_channel) noexcept override;
  void RemoveAllDataChannels() noexcept override;
  void OnDataChannelAdded(const DataChannel& data_channel) noexcept override;

  mrsResult RegisterInteropCallbacks(
      const mrsPeerConnectionInteropCallbacks& callbacks) noexcept override {
    // Make a full copy of all callbacks. Some entries might be NULL if not
    // supported by the interop.
    interop_callbacks_ = callbacks;
    return Result::kSuccess;
  }

  void OnStreamChanged(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) noexcept;

  //
  // PeerConnectionObserver interface
  //

  // Triggered when the SignalingState changed.
  void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState
                             new_state) noexcept override;

  // Triggered when media is received on a new stream from remote peer.
  void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>
                       stream) noexcept override;

  // Triggered when a remote peer closes a stream.
  void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>
                          stream) noexcept override;

  // Triggered when a remote peer opens a data channel.
  void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>
                         data_channel) noexcept override;

  /// Triggered when renegotiation is needed. For example, an ICE restart
  /// has begun, or a track has been added or removed.
  void OnRenegotiationNeeded() noexcept override;

  /// Called any time the IceConnectionState changes.
  ///
  /// From the Google implementation:
  /// "Note that our ICE states lag behind the standard slightly. The most
  /// notable differences include the fact that "failed" occurs after 15
  /// seconds, not 30, and this actually represents a combination ICE + DTLS
  /// state, so it may be "failed" if DTLS fails while ICE succeeds."
  void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState
                                 new_state) noexcept override;

  /// Called any time the IceGatheringState changes.
  void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState
                                new_state) noexcept override;

  /// A new ICE candidate has been gathered.
  void OnIceCandidate(
      const webrtc::IceCandidateInterface* candidate) noexcept override;

  /// Callback on remote track added.
  void OnAddTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
          streams) noexcept override;

  void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface>
                   transceiver) noexcept override;

  /// Callback on remote track removed.
  void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface>
                         receiver) noexcept override;

  void OnLocalDescCreated(webrtc::SessionDescriptionInterface* desc) noexcept;

  //
  // Internal
  //

  void OnLocalTrackAddedToAudioTransceiver(AudioTransceiver& transceiver,
                                           LocalAudioTrack& track) override;
  void OnLocalTrackRemovedFromAudioTransceiver(AudioTransceiver& transceiver,
                                               LocalAudioTrack& track) override;
  void OnLocalTrackAddedToVideoTransceiver(VideoTransceiver& transceiver,
                                           LocalVideoTrack& track) override;
  void OnLocalTrackRemovedFromVideoTransceiver(VideoTransceiver& transceiver,
                                               LocalVideoTrack& track) override;

 protected:
  /// The underlying PC object from the core implementation. This is NULL
  /// after |Close()| is called.
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_;

  /// Peer connection name assigned by the user. This has no meaning for the
  /// implementation.
  std::string name_;

  /// Handle to the interop wrapper associated with this object.
  mrsPeerConnectionInteropHandle interop_handle_;

  /// Callbacks used for interop management.
  mrsPeerConnectionInteropCallbacks interop_callbacks_{};

  /// User callback invoked when the peer connection received a new data channel
  /// from the remote peer and added it locally.
  DataChannelAddedCallback data_channel_added_callback_
      RTC_GUARDED_BY(data_channel_added_callback_mutex_);

  /// User callback invoked when the peer connection received a data channel
  /// remove message from the remote peer and removed it locally.
  DataChannelAddedCallback data_channel_removed_callback_
      RTC_GUARDED_BY(data_channel_removed_callback_mutex_);

  /// User callback invoked when the peer connection is established.
  /// This is generally invoked even if ICE didn't finish.
  ConnectedCallback connected_callback_
      RTC_GUARDED_BY(connected_callback_mutex_);

  /// User callback invoked when a local SDP message has been crafted by the
  /// core engine and is ready to be sent by the signaling solution.
  LocalSdpReadytoSendCallback local_sdp_ready_to_send_callback_
      RTC_GUARDED_BY(local_sdp_ready_to_send_callback_mutex_);

  /// User callback invoked when a local ICE message has been crafted by the
  /// core engine and is ready to be sent by the signaling solution.
  IceCandidateReadytoSendCallback ice_candidate_ready_to_send_callback_
      RTC_GUARDED_BY(ice_candidate_ready_to_send_callback_mutex_);

  /// User callback invoked when the ICE connection state changed.
  IceStateChangedCallback ice_state_changed_callback_
      RTC_GUARDED_BY(ice_state_changed_callback_mutex_);

  /// User callback invoked when the ICE gathering state changed.
  IceGatheringStateChangedCallback ice_gathering_state_changed_callback_
      RTC_GUARDED_BY(ice_gathering_state_changed_callback_mutex_);

  /// User callback invoked when SDP renegotiation is needed.
  RenegotiationNeededCallback renegotiation_needed_callback_
      RTC_GUARDED_BY(renegotiation_needed_callback_mutex_);

  /// User callback invoked when a remote audio track is added.
  AudioTrackAddedCallback audio_track_added_callback_
      RTC_GUARDED_BY(media_track_callback_mutex_);

  /// User callback invoked when a remote audio track is removed.
  AudioTrackRemovedCallback audio_track_removed_callback_
      RTC_GUARDED_BY(media_track_callback_mutex_);

  /// User callback invoked when a remote video track is added.
  VideoTrackAddedCallback video_track_added_callback_
      RTC_GUARDED_BY(media_track_callback_mutex_);

  /// User callback invoked when a remote video track is removed.
  VideoTrackRemovedCallback video_track_removed_callback_
      RTC_GUARDED_BY(media_track_callback_mutex_);

  std::mutex data_channel_added_callback_mutex_;
  std::mutex data_channel_removed_callback_mutex_;
  std::mutex connected_callback_mutex_;
  std::mutex local_sdp_ready_to_send_callback_mutex_;
  std::mutex ice_candidate_ready_to_send_callback_mutex_;
  std::mutex ice_state_changed_callback_mutex_;
  std::mutex ice_gathering_state_changed_callback_mutex_;
  std::mutex renegotiation_needed_callback_mutex_;
  std::mutex media_track_callback_mutex_;

  std::unordered_map<std::unique_ptr<StreamObserver>,
                     rtc::scoped_refptr<webrtc::MediaStreamInterface>>
      remote_streams_;

  /// Collection of all audio transceivers of this peer connection.
  std::vector<RefPtr<AudioTransceiver>> audio_transceivers_
      RTC_GUARDED_BY(tracks_mutex_);

  /// Collection of all video transceivers of this peer connection.
  std::vector<RefPtr<VideoTransceiver>> video_transceivers_
      RTC_GUARDED_BY(tracks_mutex_);

  /// Collection of all local audio tracks associated with this peer connection.
  std::vector<RefPtr<LocalAudioTrack>> local_audio_tracks_
      RTC_GUARDED_BY(tracks_mutex_);

  /// Collection of all local video tracks associated with this peer connection.
  std::vector<RefPtr<LocalVideoTrack>> local_video_tracks_
      RTC_GUARDED_BY(tracks_mutex_);

  /// Collection of all remote audio tracks associated with this peer
  /// connection.
  std::vector<RefPtr<RemoteAudioTrack>> remote_audio_tracks_
      RTC_GUARDED_BY(tracks_mutex_);

  /// Collection of all remote video tracks associated with this peer
  /// connection.
  std::vector<RefPtr<RemoteVideoTrack>> remote_video_tracks_
      RTC_GUARDED_BY(tracks_mutex_);

  /// Mutex for all collections of all tracks and transceivers.
  rtc::CriticalSection tracks_mutex_;

  /// Collection of all data channels associated with this peer connection.
  std::vector<std::shared_ptr<DataChannel>> data_channels_
      RTC_GUARDED_BY(data_channel_mutex_);

  /// Collection of data channels from their unique ID.
  /// This contains only data channels pre-negotiated or opened by the remote
  /// peer, as data channels opened locally won't have immediately a unique ID.
  std::unordered_map<int, std::shared_ptr<DataChannel>> data_channel_from_id_
      RTC_GUARDED_BY(data_channel_mutex_);

  /// Collection of data channels from their label.
  /// This contains only data channels with a non-empty label.
  std::unordered_multimap<str, std::shared_ptr<DataChannel>>
      data_channel_from_label_ RTC_GUARDED_BY(data_channel_mutex_);

  /// Mutex for data structures related to data channels.
  std::mutex data_channel_mutex_;

  /// Flag to indicate if SCTP was negotiated during the initial SDP handshake
  /// (m=application), which allows subsequently to use data channels. If this
  /// is false then data channels will never connnect. This is set to true if a
  /// data channel is created before the connection is established, which will
  /// force the connection to negotiate the necessary SCTP information. See
  /// https://stackoverflow.com/questions/43788872/how-are-data-channels-negotiated-between-two-peers-with-webrtc
  bool sctp_negotiated_ = true;

 private:
  PeerConnectionImpl(const PeerConnectionImpl&) = delete;
  PeerConnectionImpl& operator=(const PeerConnectionImpl&) = delete;

  /// Get an existing or create a new |AudioTransceiver| wrapper for a given RTP
  /// sender of a local audio track. The sender should have an RTP transceiver
  /// already, and this only takes care of finding/creating a wrapper for it, so
  /// should never fail as long as the sender is indeed associated with this
  /// peer connection.
  ErrorOr<RefPtr<AudioTransceiver>> GetOrCreateAudioTransceiverForSender(
      webrtc::RtpSenderInterface* sender,
      mrsAudioTransceiverInteropHandle transceiver_interop_handle);

  /// Get an existing or create a new |VideoTransceiver| wrapper for a given RTP
  /// sender of a local video track. The sender should have an RTP transceiver
  /// already, and this only takes care of finding/creating a wrapper for it, so
  /// should never fail as long as the sender is indeed associated with this
  /// peer connection.
  ErrorOr<RefPtr<VideoTransceiver>> GetOrCreateVideoTransceiverForSender(
      webrtc::RtpSenderInterface* sender,
      mrsVideoTransceiverInteropHandle transceiver_interop_handle);

  /// Get an existing or create a new |AudioTransceiver| wrapper for a given RTP
  /// receiver of a newly added remote audio track. The receiver should have an
  /// RTP transceiver already, and this only takes care of finding/creating a
  /// wrapper for it, so should never fail as long as the receiver is indeed
  /// associated with this peer connection. This is only called for new remote
  /// tracks, so will only create new transceiver wrappers for some of the new
  /// RTP transceivers. The callback on remote description applied will use
  /// |GetOrCreateTransceiver()| to create the other ones.
  ErrorOr<RefPtr<AudioTransceiver>>
  GetOrCreateAudioTransceiverForNewRemoteTrack(
      webrtc::RtpReceiverInterface* receiver);

  /// Get an existing or create a new |VideoTransceiver| wrapper for a given RTP
  /// receiver of a newly added remote video track. The sender should have an
  /// RTP transceiver already, and this only takes care of finding/creating a
  /// wrapper for it, so should never fail as long as the receiver is indeed
  /// associated with this peer connection. This is only called for new remote
  /// tracks, so will only create new transceiver wrappers for some of the new
  /// RTP transceivers. The callback on remote description applied will use
  /// |GetOrCreateTransceiver()| to create the other ones.
  ErrorOr<RefPtr<VideoTransceiver>>
  GetOrCreateVideoTransceiverForRemoteNewTrack(
      webrtc::RtpReceiverInterface* receiver);

  /// Get an existing or create a new |Transceiver| instance (either audio or
  /// video) wrapper for a given RTP transceiver just created as part of a local
  /// or remote description applied. This is always called for new transceivers,
  /// even if no remote track is added, and will create the transceiver wrappers
  /// not already created by the new remote track callbacks.
  ErrorOr<RefPtr<Transceiver>> GetOrCreateTransceiver(
      webrtc::RtpTransceiverInterface* rtp_transceiver);

  /// Create a new audio transceiver wrapper for an exist RTP transceiver
  /// missing it.
  ErrorOr<RefPtr<AudioTransceiver>> CreateAudioTransceiver(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> rtp_transceiver);

  /// Create a new video transceiver wrapper for an exist RTP transceiver
  /// missing it.
  ErrorOr<RefPtr<VideoTransceiver>> CreateVideoTransceiver(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> rtp_transceiver);
};

void StreamObserver::OnChanged() {
  owner_.OnStreamChanged(stream_);
}

class CreateSessionDescObserver
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  CreateSessionDescObserver(RefPtr<PeerConnectionImpl> peer_connection)
      : peer_connection_(
            std::forward<RefPtr<PeerConnectionImpl>>(peer_connection)) {}

  //
  // CreateSessionDescriptionObserver interface
  //

  /// This callback transfers the ownership of the |desc|.
  /// TODO(deadbeef): Make this take an std::unique_ptr<> to avoid confusion
  /// around ownership.
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) noexcept override {
    peer_connection_->OnLocalDescCreated(desc);
  }

  /// The OnFailure callback takes an RTCError, which consists of an
  /// error code and a string.
  /// RTCError is non-copyable, so it must be passed using std::move.
  /// Earlier versions of the API used a string argument. This version
  /// is deprecated; in order to let clients remove the old version, it has a
  /// default implementation. If both versions are unimplemented, the
  /// result will be a runtime error (stack overflow). This is intentional.
  void OnFailure(webrtc::RTCError error) noexcept override {}

 protected:
  RefPtr<PeerConnectionImpl> peer_connection_;
};

/// Simple observer utility delegating to a given callback on success.
class SessionDescObserver : public webrtc::SetSessionDescriptionObserver {
 public:
  SessionDescObserver() = default;
  template <typename Closure>
  SessionDescObserver(Closure&& callback)
      : callback_(std::forward<Closure>(callback)) {}
  void OnSuccess() override {
    if (callback_) {
      callback_();
    }
  }
  void OnFailure(webrtc::RTCError error) override {
    RTC_LOG(LS_ERROR) << "Error setting session description: "
                      << error.message();
  }
  void OnFailure(const std::string& error) override {
    RTC_LOG(LS_ERROR) << "Error setting session description: " << error;
  }

 protected:
  std::function<void()> callback_;
  ~SessionDescObserver() override = default;
};

struct SetRemoteSessionDescObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  SetRemoteSessionDescObserver() = default;
  template <typename Closure>
  SetRemoteSessionDescObserver(Closure&& callback)
      : callback_(std::forward<Closure>(callback)) {}
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    RTC_LOG(LS_INFO) << "Remote description set. err=" << error.message();
    if (error.ok() && callback_) {
      callback_();
    }
  }

 protected:
  std::function<void()> callback_;
};

const std::string kAudioVideoStreamId("local_av_stream");

/// The API must ensure that all strings passed to the caller are
/// null-terminated. This is a helper to ensure that calling c_str()
/// on the given std::string will yield a null-terminated string.
void ensureNullTerminatedCString(std::string& str) {
  if (str.empty() || (str.back() != '\0')) {
    str.push_back('\0');
  }
}

/// Convert an implementation value to a native API value of the ICE connection
/// state. This ensures API stability if the implementation changes, although
/// currently API values are mapped 1:1 with the implementation.
IceConnectionState IceStateFromImpl(
    webrtc::PeerConnectionInterface::IceConnectionState impl_state) {
  using Native = IceConnectionState;
  using Impl = webrtc::PeerConnectionInterface::IceConnectionState;
  static_assert((int)Native::kNew == (int)Impl::kIceConnectionNew);
  static_assert((int)Native::kChecking == (int)Impl::kIceConnectionChecking);
  static_assert((int)Native::kConnected == (int)Impl::kIceConnectionConnected);
  static_assert((int)Native::kCompleted == (int)Impl::kIceConnectionCompleted);
  static_assert((int)Native::kFailed == (int)Impl::kIceConnectionFailed);
  static_assert((int)Native::kDisconnected ==
                (int)Impl::kIceConnectionDisconnected);
  static_assert((int)Native::kClosed == (int)Impl::kIceConnectionClosed);
  return (IceConnectionState)impl_state;
}

/// Convert an implementation value to a native API value of the ICE gathering
/// state. This ensures API stability if the implementation changes, although
/// currently API values are mapped 1:1 with the implementation.
IceGatheringState IceGatheringStateFromImpl(
    webrtc::PeerConnectionInterface::IceGatheringState impl_state) {
  using Native = IceGatheringState;
  using Impl = webrtc::PeerConnectionInterface::IceGatheringState;
  static_assert((int)Native::kNew == (int)Impl::kIceGatheringNew);
  static_assert((int)Native::kGathering == (int)Impl::kIceGatheringGathering);
  static_assert((int)Native::kComplete == (int)Impl::kIceGatheringComplete);
  return (IceGatheringState)impl_state;
}

ErrorOr<RefPtr<VideoTransceiver>> PeerConnectionImpl::AddVideoTransceiver(
    const VideoTransceiverInitConfig& config) noexcept {
  if (IsClosed()) {
    return Error(Result::kInvalidOperation, "The peer connection is closed.");
  }
  RefPtr<VideoTransceiver> transceiver;
  switch (peer_->GetConfiguration().sdp_semantics) {
    case webrtc::SdpSemantics::kPlanB: {
      // Plan B doesn't have transceivers; just create a wrapper.
      transceiver =
          new VideoTransceiver(*this, config.transceiver_interop_handle);
    } break;
    case webrtc::SdpSemantics::kUnifiedPlan: {
      // Create the low-level implementation object
      webrtc::RtpTransceiverInit init{};
      init.direction = Transceiver::ToRtp(config.desired_direction);
      init.stream_ids = Transceiver::DecodeStreamIDs(config.stream_ids);
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
          ret =
              peer_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, init);
      if (!ret.ok()) {
        return ErrorFromRTCError(ret.MoveError());
      }

      // Create the transceiver wrapper
      transceiver = new VideoTransceiver(*this, ret.MoveValue(),
                                         config.transceiver_interop_handle);
    } break;
    default:
      return Error(Result::kUnknownError, "Unknown SDP semantic.");
  }
  RTC_DCHECK(transceiver);
  {
    rtc::CritScope lock(&tracks_mutex_);
    video_transceivers_.emplace_back(transceiver);
  }
  return transceiver;
}

ErrorOr<RefPtr<LocalVideoTrack>> PeerConnectionImpl::AddLocalVideoTrack(
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track,
    mrsVideoTransceiverInteropHandle transceiver_interop_handle,
    mrsLocalVideoTrackInteropHandle track_interop_handle) noexcept {
  if (IsClosed()) {
    return Error(Result::kInvalidOperation, "The peer connection is closed.");
  }
  auto result = peer_->AddTrack(video_track, {kAudioVideoStreamId});
  if (!result.ok()) {
    return ErrorFromRTCError(result.MoveError());
  }
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender = result.MoveValue();
  ErrorOr<RefPtr<VideoTransceiver>> ret =
      GetOrCreateVideoTransceiverForSender(sender, transceiver_interop_handle);
  if (!ret.ok()) {
    peer_->RemoveTrack(sender);
    return ret.MoveError();
  }
  RefPtr<VideoTransceiver> transceiver = ret.MoveValue();
  RefPtr<LocalVideoTrack> track =
      new LocalVideoTrack(*this, std::move(transceiver), std::move(video_track),
                          std::move(sender), track_interop_handle);
  {
    rtc::CritScope lock(&tracks_mutex_);
    local_video_tracks_.push_back(track);
  }
  return track;
}

Result PeerConnectionImpl::RemoveLocalVideoTrack(
    LocalVideoTrack& video_track) noexcept {
  rtc::CritScope lock(&tracks_mutex_);
  auto it = std::find_if(local_video_tracks_.begin(), local_video_tracks_.end(),
                         [&video_track](const RefPtr<LocalVideoTrack>& track) {
                           return track.get() == &video_track;
                         });
  if (it == local_video_tracks_.end()) {
    return Result::kInvalidParameter;
  }
  if (peer_) {
    video_track.RemoveFromPeerConnection(*peer_);
  }
  local_video_tracks_.erase(it);
  return Result::kSuccess;
}

void PeerConnectionImpl::RemoveLocalVideoTracksFromSource(
    ExternalVideoTrackSource& source) noexcept {
  if (!peer_) {
    return;
  }
  // Remove all tracks which share this video track source.
  // Currently there is no support for source sharing, so this should
  // amount to a single track.
  std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> senders =
      peer_->GetSenders();
  for (auto&& sender : senders) {
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track =
        sender->track();
    // Apparently track can be null if destroyed already
    //< FIXME - Is this an error?
    if (!track ||
        (track->kind() != webrtc::MediaStreamTrackInterface::kVideoKind)) {
      continue;
    }
    auto video_track = (webrtc::VideoTrackInterface*)track.get();
    if (video_track->GetSource() ==
        (webrtc::VideoTrackSourceInterface*)&source) {
      peer_->RemoveTrack(sender);
    }
  }
}

ErrorOr<RefPtr<AudioTransceiver>> PeerConnectionImpl::AddAudioTransceiver(
    const AudioTransceiverInitConfig& config) noexcept {
  if (IsClosed()) {
    return Error(Result::kInvalidOperation, "The peer connection is closed.");
  }
  RefPtr<AudioTransceiver> transceiver;
  switch (peer_->GetConfiguration().sdp_semantics) {
    case webrtc::SdpSemantics::kPlanB: {
      // Plan B doesn't have transceivers; just create a wrapper.
      transceiver =
          new AudioTransceiver(*this, config.transceiver_interop_handle);
    } break;
    case webrtc::SdpSemantics::kUnifiedPlan: {
      // Create the low-level implementation object
      webrtc::RtpTransceiverInit init{};
      init.direction = Transceiver::ToRtp(config.desired_direction);
      init.stream_ids = Transceiver::DecodeStreamIDs(config.stream_ids);
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
          ret =
              peer_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, init);
      if (!ret.ok()) {
        return ErrorFromRTCError(ret.MoveError());
      }

      // Create the transceiver wrapper
      transceiver = new AudioTransceiver(*this, ret.MoveValue(),
                                         config.transceiver_interop_handle);
    } break;
    default:
      return Error(Result::kUnknownError, "Unknown SDP semantic.");
  }
  RTC_DCHECK(transceiver);
  {
    rtc::CritScope lock(&tracks_mutex_);
    audio_transceivers_.emplace_back(transceiver);
  }
  return transceiver;
}

ErrorOr<RefPtr<LocalAudioTrack>> PeerConnectionImpl::AddLocalAudioTrack(
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track,
    mrsAudioTransceiverInteropHandle transceiver_interop_handle,
    mrsLocalAudioTrackInteropHandle track_interop_handle) noexcept {
  if (IsClosed()) {
    return Microsoft::MixedReality::WebRTC::Error(
        Result::kInvalidOperation, "The peer connection is closed.");
  }
  auto result = peer_->AddTrack(audio_track, {kAudioVideoStreamId});
  if (!result.ok()) {
    return ErrorFromRTCError(result.MoveError());
  }
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender = result.MoveValue();
  ErrorOr<RefPtr<AudioTransceiver>> ret =
      GetOrCreateAudioTransceiverForSender(sender, transceiver_interop_handle);
  if (!ret.ok()) {
    peer_->RemoveTrack(sender);
    return ret.MoveError();
  }
  RefPtr<AudioTransceiver> transceiver = ret.MoveValue();
  RefPtr<LocalAudioTrack> track =
      new LocalAudioTrack(*this, std::move(transceiver), std::move(audio_track),
                          std::move(sender), track_interop_handle);
  {
    rtc::CritScope lock(&tracks_mutex_);
    local_audio_tracks_.push_back(track);
  }
  return track;
}

Result PeerConnectionImpl::RemoveLocalAudioTrack(
    LocalAudioTrack& audio_track) noexcept {
  rtc::CritScope lock(&tracks_mutex_);
  auto it = std::find_if(local_audio_tracks_.begin(), local_audio_tracks_.end(),
                         [&audio_track](const RefPtr<LocalAudioTrack>& track) {
                           return track.get() == &audio_track;
                         });
  if (it == local_audio_tracks_.end()) {
    return Result::kInvalidParameter;
  }
  if (peer_) {
    audio_track.RemoveFromPeerConnection(*peer_);
  }
  local_audio_tracks_.erase(it);
  return Result::kSuccess;
}

ErrorOr<std::shared_ptr<DataChannel>> PeerConnectionImpl::AddDataChannel(
    int id,
    std::string_view label,
    bool ordered,
    bool reliable,
    mrsDataChannelInteropHandle dataChannelInteropHandle) noexcept {
  if (IsClosed()) {
    return Error(Result::kPeerConnectionClosed);
  }
  if (!sctp_negotiated_) {
    // Don't try to create a data channel without SCTP negotiation, it will get
    // stuck in the kConnecting state forever.
    return Error(Result::kSctpNotNegotiated);
  }
  webrtc::DataChannelInit config{};
  config.ordered = ordered;
  config.reliable = reliable;
  if (id < 0) {
    // In-band data channel with automatic ID assignment
    config.id = -1;
  } else if (id <= 0xFFFF) {
    // Out-of-band negotiated data channel with pre-established ID
    config.id = id;
  } else {
    // Valid IDs are 0-65535 (16 bits)
    return Error(Result::kOutOfRange);
  }
  std::string labelString{label};
  if (rtc::scoped_refptr<webrtc::DataChannelInterface> impl =
          peer_->CreateDataChannel(labelString, &config)) {
    // Create the native object
    auto data_channel = std::make_shared<DataChannel>(this, std::move(impl),
                                                      dataChannelInteropHandle);
    {
      auto lock = std::scoped_lock{data_channel_mutex_};
      data_channels_.push_back(data_channel);
      if (!labelString.empty()) {
        data_channel_from_label_.emplace(std::move(labelString), data_channel);
      }
      if (config.id >= 0) {
        data_channel_from_id_.try_emplace(config.id, data_channel);
      }
    }

    // For in-band channels, the creating side (here) doesn't receive an
    // OnDataChannel() message, so invoke the DataChannelAdded event right now.
    if (!data_channel->impl()->negotiated()) {
      OnDataChannelAdded(*data_channel.get());
    }

    return data_channel;
  }
  return Error(Result::kUnknownError);
}

void PeerConnectionImpl::RemoveDataChannel(
    const DataChannel& data_channel) noexcept {
  // Cache variables which require a dispatch to the signaling thread
  // to minimize the risk of a deadlock with the data channel lock below.
  const int id = data_channel.id();
  const str label = data_channel.label();

  // Move the channel to destroy out of the internal data structures
  std::shared_ptr<DataChannel> data_channel_ptr;
  {
    auto lock = std::scoped_lock{data_channel_mutex_};

    // The channel must be owned by this PeerConnection, so must be known
    // already
    auto const it = std::find_if(
        std::begin(data_channels_), std::end(data_channels_),
        [&data_channel](const std::shared_ptr<DataChannel>& other) {
          return other.get() == &data_channel;
        });
    RTC_DCHECK(it != data_channels_.end());
    // Be sure a reference is kept. This should not be a problem in theory
    // because the caller should have a reference to it, but this is safer.
    data_channel_ptr = std::move(*it);
    data_channels_.erase(it);

    // Clean-up interop maps
    auto it_id = data_channel_from_id_.find(id);
    if (it_id != data_channel_from_id_.end()) {
      data_channel_from_id_.erase(it_id);
    }
    if (!label.empty()) {
      auto it_label = data_channel_from_label_.find(label);
      if (it_label != data_channel_from_label_.end()) {
        data_channel_from_label_.erase(it_label);
      }
    }
  }

  // Close the WebRTC data channel
  webrtc::DataChannelInterface* const impl = data_channel.impl();
  impl->UnregisterObserver();  // force here, as ~DataChannel() didn't run yet
  impl->Close();

  // Invoke the DataChannelRemoved callback on the wrapper if any
  if (auto interop_handle = data_channel.GetInteropHandle()) {
    auto lock = std::scoped_lock{data_channel_removed_callback_mutex_};
    auto removed_cb = data_channel_removed_callback_;
    if (removed_cb) {
      DataChannelHandle data_native_handle = (void*)&data_channel;
      removed_cb(interop_handle, data_native_handle);
    }
  }

  // Clear the back pointer to the peer connection, and let the shared pointer
  // go out of scope and destroy the object if that was the last reference.
  data_channel_ptr->OnRemovedFromPeerConnection();
}

void PeerConnectionImpl::RemoveAllDataChannels() noexcept {
  auto lock_cb = std::scoped_lock{data_channel_removed_callback_mutex_};
  auto removed_cb = data_channel_removed_callback_;
  auto lock = std::scoped_lock{data_channel_mutex_};
  for (auto&& data_channel : data_channels_) {
    // Close the WebRTC data channel
    webrtc::DataChannelInterface* const impl = data_channel->impl();
    impl->UnregisterObserver();  // force here, as ~DataChannel() didn't run yet
    impl->Close();

    // Invoke the DataChannelRemoved callback on the wrapper if any
    if (removed_cb) {
      if (auto interop_handle = data_channel->GetInteropHandle()) {
        DataChannelHandle data_native_handle = (void*)&data_channel;
        removed_cb(interop_handle, data_native_handle);
      }
    }

    // Clear the back pointer
    data_channel->OnRemovedFromPeerConnection();
  }
  data_channel_from_id_.clear();
  data_channel_from_label_.clear();
  data_channels_.clear();
}

void PeerConnectionImpl::OnDataChannelAdded(
    const DataChannel& data_channel) noexcept {
  // The channel must be owned by this PeerConnection, so must be known already.
  // It was added in AddDataChannel() when the DataChannel object was created.
#if RTC_DCHECK_IS_ON
  {
    auto lock = std::scoped_lock{data_channel_mutex_};
    RTC_DCHECK(std::find_if(
                   data_channels_.begin(), data_channels_.end(),
                   [&data_channel](const std::shared_ptr<DataChannel>& other) {
                     return other.get() == &data_channel;
                   }) != data_channels_.end());
  }
#endif  // RTC_DCHECK_IS_ON

  // Invoke the DataChannelAdded callback on the wrapper if any
  if (auto interop_handle = data_channel.GetInteropHandle()) {
    auto lock = std::scoped_lock{data_channel_added_callback_mutex_};
    auto added_cb = data_channel_added_callback_;
    if (added_cb) {
      DataChannelHandle data_native_handle = (void*)&data_channel;
      added_cb(interop_handle, data_native_handle);
    }
  }
}

void PeerConnectionImpl::OnStreamChanged(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) noexcept {
  webrtc::AudioTrackVector audio_tracks = stream->GetAudioTracks();
  webrtc::VideoTrackVector video_tracks = stream->GetVideoTracks();
  RTC_LOG(LS_INFO) << "Media stream #" << stream->id()
                   << " changed: " << audio_tracks.size()
                   << " audio tracks and " << video_tracks.size()
                   << " video tracks.";
  // for (auto&& audio_track : remote_audio_tracks_) {
  //  if (audio_track == audio_tracks[0])
  //}
}

bool PeerConnectionImpl::AddIceCandidate(const char* sdp_mid,
                                         const int sdp_mline_index,
                                         const char* candidate) noexcept {
  if (!peer_) {
    return false;
  }
  webrtc::SdpParseError error;
  std::unique_ptr<webrtc::IceCandidateInterface> ice_candidate(
      webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, candidate, &error));
  if (!ice_candidate) {
    return false;
  }
  if (!peer_->AddIceCandidate(ice_candidate.get())) {
    return false;
  }
  return true;
}

bool PeerConnectionImpl::CreateOffer() noexcept {
  if (!peer_) {
    return false;
  }
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  /*if (mandatory_receive_)*/ {
      //< TODO - This is legacy, should use
      // transceivers
      // options.offer_to_receive_audio = true;
      // options.offer_to_receive_video = true;
  } {
    auto lock = std::scoped_lock{data_channel_mutex_};
    if (data_channels_.empty()) {
      sctp_negotiated_ = false;
    }
  }
  auto observer =
      new rtc::RefCountedObject<CreateSessionDescObserver>(this);  // 0 ref
  peer_->CreateOffer(observer, options);
  RTC_CHECK(observer->HasOneRef());  // should be == 1
  return true;
}

bool PeerConnectionImpl::CreateAnswer() noexcept {
  if (!peer_) {
    return false;
  }
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  /*if (mandatory_receive_)*/ {  //< TODO - This is legacy, should use
                                 // transceivers
    // options.offer_to_receive_audio = true;
    // options.offer_to_receive_video = true;
  }
  auto observer =
      new rtc::RefCountedObject<CreateSessionDescObserver>(this);  // 0 ref
  peer_->CreateAnswer(observer, options);
  RTC_CHECK(observer->HasOneRef());  // should be == 1
  return true;
}

void PeerConnectionImpl::Close() noexcept {
  if (!peer_) {
    return;
  }

  // Close the connection
  peer_->Close();

  {
    rtc::CritScope lock(&tracks_mutex_);

    // Remove local tracks
    while (!local_video_tracks_.empty()) {
      RefPtr<LocalVideoTrack>& ptr = local_video_tracks_.back();
      RemoveLocalVideoTrack(*ptr);
    }
    while (!local_audio_tracks_.empty()) {
      RefPtr<LocalAudioTrack>& ptr = local_audio_tracks_.back();
      RemoveLocalAudioTrack(*ptr);
    }

    // Force-remove remote tracks. It doesn't look like the TrackRemoved
    // callback is called when Close() is used, so force it here.
    auto rat = std::move(remote_audio_tracks_);
    auto rvt = std::move(remote_video_tracks_);
    auto cb_lock = std::scoped_lock{media_track_callback_mutex_};
    auto audio_cb = audio_track_removed_callback_;
    for (auto&& track : rat) {
      track->OnTrackRemoved(*this);
      if (auto interop_handle = track->GetInteropHandle()) {
        if (audio_cb) {
          auto transceiver = track->GetTransceiver();
          auto transceiver_interop_handle = transceiver->GetInteropHandle();
          audio_cb(interop_handle, track.get(), transceiver_interop_handle,
                   transceiver);
        }
      }
    }
    auto video_cb = video_track_removed_callback_;
    for (auto&& track : rvt) {
      track->OnTrackRemoved(*this);
      if (auto interop_handle = track->GetInteropHandle()) {
        if (video_cb) {
          auto transceiver = track->GetTransceiver();
          auto transceiver_interop_handle = transceiver->GetInteropHandle();
          video_cb(interop_handle, track.get(), transceiver_interop_handle,
                   transceiver);
        }
      }
    }

    // Clear transceivers
    //< TODO - This is done inside the lock, but the lock is released before
    // peer_ is cleared, so before the connection is actually closed, which
    // doesn't prevent add(Audio|Video)Transceiver from being called again in
    // parallel...
    audio_transceivers_.clear();
    video_transceivers_.clear();
  }

  remote_streams_.clear();

  RemoveAllDataChannels();

  // Release the internal webrtc::PeerConnection implementation. This call will
  // get proxied to the WebRTC signaling thread, so needs to occur before the
  // global factory shuts down and terminates the threads, which potentially
  // happens just after this call when called from the destructor if this is the
  // last object alive. This is also used as a marker for |IsClosed()|.
  peer_ = nullptr;
}

bool PeerConnectionImpl::IsClosed() const noexcept {
  return (peer_ == nullptr);
}

bool PeerConnectionImpl::SetRemoteDescriptionAsync(
    const char* type,
    const char* sdp,
    Callback<> callback) noexcept {
  if (!peer_) {
    return false;
  }
  {
    auto lock = std::scoped_lock{data_channel_mutex_};
    if (data_channels_.empty()) {
      sctp_negotiated_ = false;
    }
  }
  std::string sdp_type_str(type);
  auto sdp_type = webrtc::SdpTypeFromString(sdp_type_str);
  if (!sdp_type.has_value())
    return false;
  std::string remote_desc(sdp);
  webrtc::SdpParseError error;
  std::unique_ptr<webrtc::SessionDescriptionInterface> session_description(
      webrtc::CreateSessionDescription(sdp_type.value(), remote_desc, &error));
  if (!session_description)
    return false;
  rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface> observer =
      new rtc::RefCountedObject<SetRemoteSessionDescObserver>([this, callback] {
        // Inspect transceiver directions, check for changes to update the
        // interop layer with the actually negotiated direction.
        std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
            changed_transceivers;
        for (auto&& tr : peer_->GetTransceivers()) {
          ErrorOr<RefPtr<Transceiver>> err = GetOrCreateTransceiver(tr);
          RTC_DCHECK(err.ok());
          err.value()->OnSessionDescUpdated(/*remote=*/true);
        }
        // Fire completed callback to signal remote description was applied.
        callback();
      });
  peer_->SetRemoteDescription(std::move(session_description),
                              std::move(observer));
  return true;
}

void PeerConnectionImpl::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) noexcept {
  // See https://w3c.github.io/webrtc-pc/#rtcsignalingstate-enum
  switch (new_state) {
    case webrtc::PeerConnectionInterface::kStable:
      // Transitioning *to* stable means final answer received.
      // Otherwise the only possible way to be in the stable state is at start,
      // but this callback would not be invoked then because there's no
      // transition.
      {
        auto lock = std::scoped_lock{connected_callback_mutex_};
        connected_callback_();
      }
      break;
    case webrtc::PeerConnectionInterface::kHaveLocalOffer:
      break;
    case webrtc::PeerConnectionInterface::kHaveLocalPrAnswer:
      break;
    case webrtc::PeerConnectionInterface::kHaveRemoteOffer:
      break;
    case webrtc::PeerConnectionInterface::kHaveRemotePrAnswer:
      break;
  }
}

void PeerConnectionImpl::OnAddStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) noexcept {
  RTC_LOG(LS_INFO) << "Added stream #" << stream->id() << " with "
                   << stream->GetAudioTracks().size() << " audio tracks and "
                   << stream->GetVideoTracks().size() << " video tracks.";
  auto observer = std::make_unique<StreamObserver>(*this, stream);
  stream->RegisterObserver(observer.get());
  remote_streams_.emplace(std::move(observer), stream);
}

void PeerConnectionImpl::OnRemoveStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) noexcept {
  RTC_LOG(LS_INFO) << "Removed stream #" << stream->id() << " with "
                   << stream->GetAudioTracks().size() << " audio tracks and "
                   << stream->GetVideoTracks().size() << " video tracks.";
  auto it = std::find_if(
      remote_streams_.begin(), remote_streams_.end(),
      [&stream](
          std::pair<const std::unique_ptr<StreamObserver>,
                    rtc::scoped_refptr<webrtc::MediaStreamInterface>>& pair) {
        return pair.second == stream;
      });
  if (it == remote_streams_.end()) {
    return;
  }
  stream->UnregisterObserver(it->first.get());
  remote_streams_.erase(it);
}

void PeerConnectionImpl::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> impl) noexcept {
  // If receiving a new data channel, then obviously SCTP has been negotiated so
  // it is safe to create other ones.
  sctp_negotiated_ = true;

  // Read the data channel config
  std::string label = impl->label();
  mrsDataChannelConfig config;
  config.id = impl->id();
  config.label = label.c_str();
  if (impl->ordered()) {
    config.flags = (mrsDataChannelConfigFlags)(
        (uint32_t)config.flags | (uint32_t)mrsDataChannelConfigFlags::kOrdered);
  }
  if (impl->reliable()) {
    config.flags = (mrsDataChannelConfigFlags)(
        (uint32_t)config.flags |
        (uint32_t)mrsDataChannelConfigFlags::kReliable);
  }

  // Create an interop wrapper for the new native object if needed
  mrsDataChannelInteropHandle data_channel_interop_handle{};
  mrsDataChannelCallbacks callbacks{};
  if (auto create_cb = interop_callbacks_.data_channel_create_object) {
    data_channel_interop_handle =
        (*create_cb)(interop_handle_, config, &callbacks);
  }

  // Create a new native object
  auto data_channel =
      std::make_shared<DataChannel>(this, impl, data_channel_interop_handle);
  {
    auto lock = std::scoped_lock{data_channel_mutex_};
    data_channels_.push_back(data_channel);
    if (!label.empty()) {
      // Move |label| into the map to avoid copy
      auto it =
          data_channel_from_label_.emplace(std::move(label), data_channel);
      // Update the address to the moved item in case it changed
      config.label = it->first.c_str();
    }
    if (data_channel->id() >= 0) {
      data_channel_from_id_.try_emplace(data_channel->id(), data_channel);
    }
  }

  // TODO -- Invoke some callback on the C++ side

  if (data_channel_interop_handle) {
    // Register the interop callbacks
    data_channel->SetMessageCallback(DataChannel::MessageCallback{
        callbacks.message_callback, callbacks.message_user_data});
    data_channel->SetBufferingCallback(DataChannel::BufferingCallback{
        callbacks.buffering_callback, callbacks.buffering_user_data});
    data_channel->SetStateCallback(DataChannel::StateCallback{
        callbacks.state_callback, callbacks.state_user_data});

    // Invoke the DataChannelAdded callback on the wrapper
    {
      auto lock = std::scoped_lock{data_channel_added_callback_mutex_};
      auto added_cb = data_channel_added_callback_;
      if (added_cb) {
        const DataChannelHandle data_native_handle = data_channel.get();
        added_cb(data_channel_interop_handle, data_native_handle);
      }
    }
  }
}

void PeerConnectionImpl::OnRenegotiationNeeded() noexcept {
  auto lock = std::scoped_lock{renegotiation_needed_callback_mutex_};
  auto cb = renegotiation_needed_callback_;
  if (cb) {
    cb();
  }
}

void PeerConnectionImpl::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) noexcept {
  auto lock = std::scoped_lock{ice_state_changed_callback_mutex_};
  auto cb = ice_state_changed_callback_;
  if (cb) {
    cb(IceStateFromImpl(new_state));
  }
}

void PeerConnectionImpl::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) noexcept {
  auto lock = std::scoped_lock{ice_gathering_state_changed_callback_mutex_};
  auto cb = ice_gathering_state_changed_callback_;
  if (cb) {
    cb(IceGatheringStateFromImpl(new_state));
  }
}

void PeerConnectionImpl::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) noexcept {
  auto lock = std::scoped_lock{ice_candidate_ready_to_send_callback_mutex_};
  auto cb = ice_candidate_ready_to_send_callback_;
  if (cb) {
    std::string sdp;
    if (!candidate->ToString(&sdp))
      return;
    ensureNullTerminatedCString(sdp);
    std::string sdp_mid = candidate->sdp_mid();
    ensureNullTerminatedCString(sdp_mid);
    cb(sdp.c_str(), candidate->sdp_mline_index(), sdp_mid.c_str());
  }
}

void PeerConnectionImpl::OnAddTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
    /*streams*/) noexcept {
  RTC_LOG(LS_INFO) << "Added receiver #" << receiver->id() << " of type "
                   << (int)receiver->media_type();
  for (auto&& stream : receiver->streams()) {
    RTC_LOG(LS_INFO) << "+ Track #" << receiver->track()->id()
                     << " with stream #" << stream->id();
  }

  // Create the remote track wrapper
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track =
      receiver->track();
  const std::string& track_name = track->id();
  const std::string& track_kind_str = track->kind();
  if (track_kind_str == webrtc::MediaStreamTrackInterface::kAudioKind) {
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
        static_cast<webrtc::AudioTrackInterface*>(track.release()));

    // Create an interop wrapper for the new native object if needed
    mrsRemoteAudioTrackInteropHandle interop_handle{};
    if (auto create_cb = interop_callbacks_.remote_audio_track_create_object) {
      mrsRemoteAudioTrackConfig config;
      config.track_name = track_name.c_str();
      interop_handle = (*create_cb)(interop_handle_, config);
    }

    // Get or create the transceiver wrapper based on the RTP receiver. Because
    // this callback is fired before the one at the end of the remote
    // description being applied, the transceiver wrappers for the newly added
    // RTP transceivers have not been created yet, so create them here.
    auto ret = GetOrCreateAudioTransceiverForNewRemoteTrack(receiver);
    if (!ret.ok()) {
      return;
    }
    RefPtr<AudioTransceiver> transceiver = ret.MoveValue();

    // The transceiver wrapper might have been created, in which case we need to
    // inform its interop wrapper of its handle.
    mrsAudioTransceiverInteropHandle transceiver_interop_handle =
        transceiver->GetInteropHandle();

    // Create the native object
    RefPtr<RemoteAudioTrack> remote_audio_track =
        new RemoteAudioTrack(*this, transceiver, std::move(audio_track),
                             std::move(receiver), interop_handle);
    {
      rtc::CritScope lock(&tracks_mutex_);
      remote_audio_tracks_.emplace_back(remote_audio_track);
    }

    // Invoke the AudioTrackAdded callback, which will set the native handle on
    // the interop wrapper (if created above)
    {
      auto lock = std::scoped_lock{media_track_callback_mutex_};
      auto cb = audio_track_added_callback_;
      if (cb) {
        AudioTransceiverHandle tranceiver_handle = transceiver.release();
        RemoteAudioTrackHandle audio_handle = remote_audio_track.release();
        cb(interop_handle, audio_handle, transceiver_interop_handle,
           tranceiver_handle);
      }
    }
  } else if (track_kind_str == webrtc::MediaStreamTrackInterface::kVideoKind) {
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
        static_cast<webrtc::VideoTrackInterface*>(track.release()));

    // Create an interop wrapper for the new native object if needed
    mrsRemoteVideoTrackInteropHandle interop_handle{};
    if (auto create_cb = interop_callbacks_.remote_video_track_create_object) {
      mrsRemoteVideoTrackConfig config;
      config.track_name = track_name.c_str();
      interop_handle = (*create_cb)(interop_handle_, config);
    }

    // Get or create the transceiver wrapper based on the RTP receiver. Because
    // this callback is fired before the one at the end of the remote
    // description being applied, the transceiver wrappers for the newly added
    // RTP transceivers have not been created yet, so create them here.
    auto ret = GetOrCreateVideoTransceiverForRemoteNewTrack(receiver);
    if (!ret.ok()) {
      return;
    }
    RefPtr<VideoTransceiver> transceiver = ret.MoveValue();

    // The transceiver wrapper might have been created, in which case we need to
    // inform its interop wrapper of its handle.
    mrsVideoTransceiverInteropHandle transceiver_interop_handle =
        transceiver->GetInteropHandle();

    // Create the native object
    RefPtr<RemoteVideoTrack> remote_video_track =
        new RemoteVideoTrack(*this, transceiver, std::move(video_track),
                             std::move(receiver), interop_handle);
    {
      rtc::CritScope lock(&tracks_mutex_);
      remote_video_tracks_.emplace_back(remote_video_track);
    }

    // Invoke the VideoTrackAdded callback, which will set the native handle on
    // the interop wrapper (if created above)
    {
      auto lock = std::scoped_lock{media_track_callback_mutex_};
      auto cb = video_track_added_callback_;
      if (cb) {
        VideoTransceiverHandle tranceiver_handle = transceiver.release();
        RemoteVideoTrackHandle video_handle = remote_video_track.release();
        cb(interop_handle, video_handle, transceiver_interop_handle,
           tranceiver_handle);
      }
    }
  }
}

void PeerConnectionImpl::OnTrack(
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) noexcept {
  RTC_LOG(LS_INFO) << "Added transceiver mid=#" << transceiver->mid().value()
                   << " of type " << (int)transceiver->media_type()
                   << " with desired direction "
                   << (int)transceiver->direction();
  auto receiver = transceiver->receiver();
  if (auto track = receiver->track()) {
    RTC_LOG(LS_INFO) << "Recv with track #" << track->id()
                     << " enabled=" << track->enabled();
  } else {
    RTC_LOG(LS_INFO) << "Recv with NULL track";
  }
  for (auto&& id : receiver->stream_ids()) {
    RTC_LOG(LS_INFO) << "+ Stream #" << id;
  }
  auto sender = transceiver->sender();
  if (auto track = sender->track()) {
    RTC_LOG(LS_INFO) << "Send #" << track->id()
                     << " enabled=" << track->enabled();
  } else {
    RTC_LOG(LS_INFO) << "Send with NULL track";
  }
  for (auto&& id : sender->stream_ids()) {
    RTC_LOG(LS_INFO) << "+ Stream #" << id;
  }
}

void PeerConnectionImpl::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) noexcept {
  RTC_LOG(LS_INFO) << "Removed track #" << receiver->id() << " of type "
                   << (int)receiver->media_type();
  for (auto&& stream : receiver->streams()) {
    RTC_LOG(LS_INFO) << "- Track #" << receiver->id() << " with stream #"
                     << stream->id();
  }

  // Unregister the remote observer
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track =
      receiver->track();
  const std::string& track_kind_str = track->kind();
  if (track_kind_str == webrtc::MediaStreamTrackInterface::kAudioKind) {
    rtc::CritScope tracks_lock(&tracks_mutex_);
    auto it =
        std::find_if(remote_audio_tracks_.begin(), remote_audio_tracks_.end(),
                     [&receiver](const RefPtr<RemoteAudioTrack>& remote_track) {
                       return (remote_track->receiver() == receiver);
                     });
    if (it == remote_audio_tracks_.end()) {
      return;
    }
    RefPtr<RemoteAudioTrack> audio_track = std::move(*it);
    RefPtr<AudioTransceiver> audio_transceiver = audio_track->GetTransceiver();
    remote_audio_tracks_.erase(it);
    audio_track->OnTrackRemoved(*this);

    // Invoke the TrackRemoved callback
    if (auto interop_handle = audio_track->GetInteropHandle()) {
      auto lock = std::scoped_lock{media_track_callback_mutex_};
      auto cb = audio_track_removed_callback_;
      if (cb) {
        auto transceiver_interop_handle = audio_transceiver->GetInteropHandle();
        cb(interop_handle, audio_track.get(), transceiver_interop_handle,
           audio_transceiver.get());
      }
    }
    // |audio_track| goes out of scope and destroys the C++ instance
  } else if (track_kind_str == webrtc::MediaStreamTrackInterface::kVideoKind) {
    rtc::CritScope tracks_lock(&tracks_mutex_);
    auto it =
        std::find_if(remote_video_tracks_.begin(), remote_video_tracks_.end(),
                     [&receiver](const RefPtr<RemoteVideoTrack>& remote_track) {
                       return (remote_track->receiver() == receiver);
                     });
    if (it == remote_video_tracks_.end()) {
      return;
    }
    RefPtr<RemoteVideoTrack> video_track = std::move(*it);
    RefPtr<VideoTransceiver> video_transceiver = video_track->GetTransceiver();
    remote_video_tracks_.erase(it);
    video_track->OnTrackRemoved(*this);

    // Invoke the TrackRemoved callback
    if (auto interop_handle = video_track->GetInteropHandle()) {
      auto lock = std::scoped_lock{media_track_callback_mutex_};
      auto cb = video_track_removed_callback_;
      if (cb) {
        auto transceiver_interop_handle = video_transceiver->GetInteropHandle();
        cb(interop_handle, video_track.get(), transceiver_interop_handle,
           video_transceiver.get());
      }
    }
    // |video_track| goes out of scope and destroys the C++ instance
  }
}

void PeerConnectionImpl::OnLocalDescCreated(
    webrtc::SessionDescriptionInterface* desc) noexcept {
  if (!peer_) {
    return;
  }
  rtc::scoped_refptr<webrtc::SetSessionDescriptionObserver> observer =
      new rtc::RefCountedObject<SessionDescObserver>([this] {
        // Inspect transceiver directions, check for changes to update the
        // interop layer with the actually negotiated direction.
        std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
            changed_transceivers;
        for (auto&& tr : peer_->GetTransceivers()) {
          ErrorOr<RefPtr<Transceiver>> err = GetOrCreateTransceiver(tr);
          RTC_DCHECK(err.ok());
          err.value()->OnSessionDescUpdated(/*remote=*/false);
        }

        // Fire interop callback, if any
        {
          auto lock = std::scoped_lock{local_sdp_ready_to_send_callback_mutex_};
          if (auto cb = local_sdp_ready_to_send_callback_) {
            auto desc = peer_->local_description();
            std::string type{SdpTypeToString(desc->GetType())};
            std::string sdp;
            desc->ToString(&sdp);
            cb(type.c_str(), sdp.c_str());
          }
        }
      });
  // SetLocalDescription will invoke observer.OnSuccess() once done, which
  // will in turn invoke the |local_sdp_ready_to_send_callback_| registered if
  // any, or do nothing otherwise. The observer is a mandatory parameter.
  peer_->SetLocalDescription(observer, desc);
}

void PeerConnectionImpl::OnLocalTrackAddedToAudioTransceiver(
    AudioTransceiver& transceiver,
    LocalAudioTrack& track) {
  rtc::CritScope lock(&tracks_mutex_);
  RTC_DCHECK(std::find_if(audio_transceivers_.begin(),
                          audio_transceivers_.end(),
                          [&transceiver](const RefPtr<AudioTransceiver>& tr) {
                            return (tr.get() == &transceiver);
                          }) != audio_transceivers_.end());
  RTC_DCHECK(std::find_if(local_audio_tracks_.begin(),
                          local_audio_tracks_.end(),
                          [&track](const RefPtr<LocalAudioTrack>& tr) {
                            return (tr.get() == &track);
                          }) == local_audio_tracks_.end());
  local_audio_tracks_.push_back(&track);
}

void PeerConnectionImpl::OnLocalTrackRemovedFromAudioTransceiver(
    AudioTransceiver& transceiver,
    LocalAudioTrack& track) {
  rtc::CritScope lock(&tracks_mutex_);
  RTC_DCHECK(std::find_if(audio_transceivers_.begin(),
                          audio_transceivers_.end(),
                          [&transceiver](const RefPtr<AudioTransceiver>& tr) {
                            return (tr.get() == &transceiver);
                          }) != audio_transceivers_.end());
  auto it = std::find_if(local_audio_tracks_.begin(), local_audio_tracks_.end(),
                         [&track](const RefPtr<LocalAudioTrack>& tr) {
                           return (tr.get() == &track);
                         });
  RTC_DCHECK(it != local_audio_tracks_.end());
  local_audio_tracks_.erase(it);
}

void PeerConnectionImpl::OnLocalTrackAddedToVideoTransceiver(
    VideoTransceiver& transceiver,
    LocalVideoTrack& track) {
  rtc::CritScope lock(&tracks_mutex_);
  RTC_DCHECK(std::find_if(video_transceivers_.begin(),
                          video_transceivers_.end(),
                          [&transceiver](const RefPtr<VideoTransceiver>& tr) {
                            return (tr.get() == &transceiver);
                          }) != video_transceivers_.end());
  RTC_DCHECK(std::find_if(local_video_tracks_.begin(),
                          local_video_tracks_.end(),
                          [&track](const RefPtr<LocalVideoTrack>& tr) {
                            return (tr.get() == &track);
                          }) == local_video_tracks_.end());
  local_video_tracks_.push_back(&track);
}

void PeerConnectionImpl::OnLocalTrackRemovedFromVideoTransceiver(
    VideoTransceiver& transceiver,
    LocalVideoTrack& track) {
  rtc::CritScope lock(&tracks_mutex_);
  RTC_DCHECK(std::find_if(video_transceivers_.begin(),
                          video_transceivers_.end(),
                          [&transceiver](const RefPtr<VideoTransceiver>& tr) {
                            return (tr.get() == &transceiver);
                          }) != video_transceivers_.end());
  auto it = std::find_if(local_video_tracks_.begin(), local_video_tracks_.end(),
                         [&track](const RefPtr<LocalVideoTrack>& tr) {
                           return (tr.get() == &track);
                         });
  RTC_DCHECK(it != local_video_tracks_.end());
  local_video_tracks_.erase(it);
}

ErrorOr<RefPtr<AudioTransceiver>>
PeerConnectionImpl::GetOrCreateAudioTransceiverForSender(
    webrtc::RtpSenderInterface* sender,
    mrsAudioTransceiverInteropHandle transceiver_interop_handle) {
  RTC_DCHECK(sender->media_type() == cricket::MediaType::MEDIA_TYPE_AUDIO);

  // Find existing transceiver for sender
  auto it = std::find_if(audio_transceivers_.begin(), audio_transceivers_.end(),
                         [sender](const RefPtr<AudioTransceiver>& tr) {
                           return (tr->impl()->sender() == sender);
                         });
  if (it != audio_transceivers_.end()) {
    RTC_DCHECK_EQ(transceiver_interop_handle, it->get()->GetInteropHandle());
    return RefPtr<AudioTransceiver>(*it);
  }

  // Create new transceiver for audio track
  RefPtr<AudioTransceiver> transceiver;
  switch (peer_->GetConfiguration().sdp_semantics) {
    case webrtc::SdpSemantics::kPlanB: {
      assert(false);  //< TODO...
    } break;
    case webrtc::SdpSemantics::kUnifiedPlan: {
      // Find transceiver implementation. It doesn't seem like there is a direct
      // back-link, so iterate over all the peer connection transceivers.
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> impl;
      for (auto&& tr : peer_->GetTransceivers()) {
        if (tr->sender() == sender) {
          impl = tr;
          break;
        }
      }
      if (!impl) {
        return Error(Result::kInvalidOperation,
                     "Cannot match sender with transceiver.");
      }

      // Create the transceiver wrapper
      transceiver = new AudioTransceiver(*this, std::move(impl),
                                         transceiver_interop_handle);

      // Note: at this point the native wrapper knows about the interop wrapper,
      // but not the opposite. Normally we'd fire another "created-callback"
      // with the native wrapper handle to sync the interop wrapper, but here it
      // is being created as part of a local track creation, so we bundle that
      // with the "track-created" event.
    } break;
    default:
      return Error(Result::kUnknownError, "Unknown SDP semantic");
  }
  if (transceiver) {
    rtc::CritScope lock(&tracks_mutex_);
    audio_transceivers_.push_back(transceiver);
    return transceiver;
  }
  return Error(Result::kUnknownError,
               "Failed to create a new transceiver for local audio track.");
}

ErrorOr<RefPtr<VideoTransceiver>>
PeerConnectionImpl::GetOrCreateVideoTransceiverForSender(
    webrtc::RtpSenderInterface* sender,
    mrsVideoTransceiverInteropHandle transceiver_interop_handle) {
  RTC_DCHECK(sender->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO);

  // Find existing transceiver for sender
  auto it = std::find_if(video_transceivers_.begin(), video_transceivers_.end(),
                         [sender](const RefPtr<VideoTransceiver>& tr) {
                           return (tr->impl()->sender() == sender);
                         });
  if (it != video_transceivers_.end()) {
    RTC_DCHECK_EQ(transceiver_interop_handle, it->get()->GetInteropHandle());
    return RefPtr<VideoTransceiver>(*it);
  }

  // Create new transceiver for video track
  RefPtr<VideoTransceiver> transceiver;
  switch (peer_->GetConfiguration().sdp_semantics) {
    case webrtc::SdpSemantics::kPlanB: {
      assert(false);  //< TODO...
    } break;
    case webrtc::SdpSemantics::kUnifiedPlan: {
      // Find transceiver implementation. It doesn't seem like there is a direct
      // back-link, so iterate over all the peer connection transceivers.
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> impl;
      for (auto&& tr : peer_->GetTransceivers()) {
        if (tr->sender() == sender) {
          impl = tr;
          break;
        }
      }
      if (!impl) {
        return Error(Result::kInvalidOperation,
                     "Cannot match sender with transceiver.");
      }

      // Create the transceiver wrapper
      transceiver = new VideoTransceiver(*this, std::move(impl),
                                         transceiver_interop_handle);

      // Note: at this point the native wrapper knows about the interop wrapper,
      // but not the opposite. Normally we'd fire another "created-callback"
      // with the native wrapper handle to sync the interop wrapper, but here it
      // is being created as part of a local track creation, so we bundle that
      // with the "track-created" event.
    } break;
    default:
      return Error(Result::kUnknownError, "Unknown SDP semantic");
  }
  if (transceiver) {
    rtc::CritScope lock(&tracks_mutex_);
    video_transceivers_.push_back(transceiver);
    return transceiver;
  }
  return Error(Result::kUnknownError,
               "Failed to create a new transceiver for local video track.");
}

ErrorOr<RefPtr<AudioTransceiver>>
PeerConnectionImpl::GetOrCreateAudioTransceiverForNewRemoteTrack(
    webrtc::RtpReceiverInterface* receiver) {
  RTC_DCHECK(receiver->media_type() == cricket::MediaType::MEDIA_TYPE_AUDIO);

  // Try to find an existing audio transceiver wrapper for the given RTP
  // receiver of the remote track.
  {
    auto it_tr =
        std::find_if(audio_transceivers_.begin(), audio_transceivers_.end(),
                     [receiver](const RefPtr<AudioTransceiver>& tr) {
                       return (tr->impl()->receiver() == receiver);
                     });
    if (it_tr != audio_transceivers_.end()) {
      return RefPtr<AudioTransceiver>(*it_tr);
    }
  }

  // Find the RTP transceiver of the RTP receiver
  auto transceivers = peer_->GetTransceivers();
  auto it_impl = std::find_if(
      transceivers.begin(), transceivers.end(),
      [receiver](auto&& tr) { return tr->receiver() == receiver; });
  if (it_impl == transceivers.end()) {
    return Error(
        Result::kNotFound,
        "Failed to match RTP receiver with an existing RTP transceiver.");
  }
  rtc::scoped_refptr<webrtc::RtpTransceiverInterface> impl = *it_impl;

  // Create a new audio transceiver wrapper for it
  return CreateAudioTransceiver(std::move(impl));
}

ErrorOr<RefPtr<VideoTransceiver>>
PeerConnectionImpl::GetOrCreateVideoTransceiverForRemoteNewTrack(
    webrtc::RtpReceiverInterface* receiver) {
  RTC_DCHECK(receiver->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO);

  // Try to find an existing video transceiver wrapper for the given RTP
  // receiver of the remote track.
  {
    auto it_tr =
        std::find_if(video_transceivers_.begin(), video_transceivers_.end(),
                     [receiver](const RefPtr<VideoTransceiver>& tr) {
                       return (tr->impl()->receiver() == receiver);
                     });
    if (it_tr != video_transceivers_.end()) {
      return RefPtr<VideoTransceiver>(*it_tr);
    }
  }

  // Find the RTP transceiver of the RTP receiver
  auto transceivers = peer_->GetTransceivers();
  auto it_impl = std::find_if(
      transceivers.begin(), transceivers.end(),
      [receiver](auto&& tr) { return tr->receiver() == receiver; });
  if (it_impl == transceivers.end()) {
    return Error(
        Result::kNotFound,
        "Failed to match RTP receiver with an existing RTP transceiver.");
  }
  rtc::scoped_refptr<webrtc::RtpTransceiverInterface> impl = *it_impl;

  // Create a new video transceiver wrapper for it
  return CreateVideoTransceiver(std::move(impl));
}

ErrorOr<RefPtr<Transceiver>> PeerConnectionImpl::GetOrCreateTransceiver(
    webrtc::RtpTransceiverInterface* rtp_transceiver) {
  switch (rtp_transceiver->media_type()) {
    case cricket::MediaType::MEDIA_TYPE_AUDIO: {
      // Find an existing transceiver wrapper which would have been created just
      // a moment ago by the remote track added callback.
      rtc::CritScope lock(&tracks_mutex_);
      for (auto&& tr : audio_transceivers_) {
        if (tr->impl() == rtp_transceiver) {
          return RefPtr<Transceiver>(tr);
        }
      }
      // Not found - create a new one
      return CreateAudioTransceiver(rtp_transceiver);
    };

    case cricket::MediaType::MEDIA_TYPE_VIDEO: {
      // Find an existing transceiver wrapper which would have been created just
      // a moment ago by the remote track added callback.
      rtc::CritScope lock(&tracks_mutex_);
      for (auto&& tr : video_transceivers_) {
        if (tr->impl() == rtp_transceiver) {
          return RefPtr<Transceiver>(tr);
        }
      }
      // Not found - create a new one
      return CreateVideoTransceiver(rtp_transceiver);
    };

    default:
      return Error(Result::kUnknownError, "Unknown SDP semantic");
  }
}

ErrorOr<RefPtr<AudioTransceiver>> PeerConnectionImpl::CreateAudioTransceiver(
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> rtp_transceiver) {
  // Create an interop wrapper for the new native object if needed
  mrsAudioTransceiverInteropHandle interop_handle{};
  if (auto create_cb = interop_callbacks_.audio_transceiver_create_object) {
    mrsAudioTransceiverConfig config{};
    config.initial_desired_direction =
        Transceiver::FromRtp(rtp_transceiver->direction());
    interop_handle = (*create_cb)(interop_handle_, config);
  }

  // Create new transceiver wrapper
  RefPtr<AudioTransceiver> transceiver;
  switch (peer_->GetConfiguration().sdp_semantics) {
    case webrtc::SdpSemantics::kPlanB: {
      assert(false);  //< TODO...
    } break;
    case webrtc::SdpSemantics::kUnifiedPlan: {
      // Create the transceiver wrapper
      transceiver = new AudioTransceiver(*this, std::move(rtp_transceiver),
                                         interop_handle);

      // Synchronize the interop wrapper with the current object.
      if (auto cb = interop_callbacks_.audio_transceiver_finish_create) {
        transceiver->AddRef();
        cb(interop_handle, transceiver.get());
      }
    } break;
    default:
      return Error(Result::kUnknownError, "Unknown SDP semantic");
  }
  if (transceiver) {
    rtc::CritScope lock(&tracks_mutex_);
    audio_transceivers_.push_back(transceiver);
  }
  return transceiver;
}

ErrorOr<RefPtr<VideoTransceiver>> PeerConnectionImpl::CreateVideoTransceiver(
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> rtp_transceiver) {
  // Create an interop wrapper for the new native object if needed
  mrsVideoTransceiverInteropHandle interop_handle{};
  if (auto create_cb = interop_callbacks_.video_transceiver_create_object) {
    mrsVideoTransceiverConfig config{};
    config.initial_desired_direction =
        Transceiver::FromRtp(rtp_transceiver->direction());
    interop_handle = (*create_cb)(interop_handle_, config);
  }

  // Create new transceiver wrapper
  RefPtr<VideoTransceiver> transceiver;
  switch (peer_->GetConfiguration().sdp_semantics) {
    case webrtc::SdpSemantics::kPlanB: {
      assert(false);  //< TODO...
    } break;
    case webrtc::SdpSemantics::kUnifiedPlan: {
      // Create the transceiver wrapper
      transceiver = new VideoTransceiver(*this, std::move(rtp_transceiver),
                                         interop_handle);

      // Synchronize the interop wrapper with the current object.
      if (auto cb = interop_callbacks_.video_transceiver_finish_create) {
        transceiver->AddRef();
        cb(interop_handle, transceiver.get());
      }
    } break;
    default:
      return Error(Result::kUnknownError, "Unknown SDP semantic");
  }
  if (transceiver) {
    rtc::CritScope lock(&tracks_mutex_);
    video_transceivers_.push_back(transceiver);
  }
  return transceiver;
}

webrtc::PeerConnectionInterface::IceTransportsType ICETransportTypeToNative(
    IceTransportType mrsValue) {
  using Native = webrtc::PeerConnectionInterface::IceTransportsType;
  using Impl = IceTransportType;
  static_assert((int)Native::kNone == (int)Impl::kNone);
  static_assert((int)Native::kNoHost == (int)Impl::kNoHost);
  static_assert((int)Native::kRelay == (int)Impl::kRelay);
  static_assert((int)Native::kAll == (int)Impl::kAll);
  return static_cast<Native>(mrsValue);
}

webrtc::PeerConnectionInterface::BundlePolicy BundlePolicyToNative(
    BundlePolicy mrsValue) {
  using Native = webrtc::PeerConnectionInterface::BundlePolicy;
  using Impl = BundlePolicy;
  static_assert((int)Native::kBundlePolicyBalanced == (int)Impl::kBalanced);
  static_assert((int)Native::kBundlePolicyMaxBundle == (int)Impl::kMaxBundle);
  static_assert((int)Native::kBundlePolicyMaxCompat == (int)Impl::kMaxCompat);
  return static_cast<Native>(mrsValue);
}

}  // namespace

namespace Microsoft::MixedReality::WebRTC {

ErrorOr<RefPtr<PeerConnection>> PeerConnection::create(
    const PeerConnectionConfiguration& config,
    mrsPeerConnectionInteropHandle interop_handle) {
  // Set the default value for the HL1 workaround before creating any
  // connection. This has no effect on other platforms.
  SetFrameHeightRoundMode(FrameHeightRoundMode::kCrop);

  // Ensure the factory exists
  RefPtr<GlobalFactory> global_factory = GlobalFactory::InstancePtr();
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory =
      global_factory->GetPeerConnectionFactory();
  if (!factory) {
    return Error(Result::kUnknownError);
  }

  // Setup the connection configuration
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  if (config.encoded_ice_servers != nullptr) {
    std::string encoded_ice_servers{config.encoded_ice_servers};
    rtc_config.servers = DecodeIceServers(encoded_ice_servers);
  }
  rtc_config.enable_rtp_data_channel = false;  // Always false for security
  rtc_config.enable_dtls_srtp = true;          // Always true for security
  rtc_config.type = ICETransportTypeToNative(config.ice_transport_type);
  rtc_config.bundle_policy = BundlePolicyToNative(config.bundle_policy);
  rtc_config.sdp_semantics = (config.sdp_semantic == SdpSemantic::kUnifiedPlan
                                  ? webrtc::SdpSemantics::kUnifiedPlan
                                  : webrtc::SdpSemantics::kPlanB);
  auto peer = new PeerConnectionImpl(interop_handle);
  webrtc::PeerConnectionDependencies dependencies(peer);
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> impl =
      factory->CreatePeerConnection(rtc_config, std::move(dependencies));
  if (impl.get() == nullptr) {
    return Error(Result::kUnknownError);
  }
  peer->SetPeerImpl(std::move(impl));
  return RefPtr<PeerConnection>(peer);
}

}  // namespace Microsoft::MixedReality::WebRTC

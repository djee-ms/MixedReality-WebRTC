// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// This is a precompiled header, it must be on its own, followed by a blank
// line, to prevent clang-format from reordering it with other headers.
#include "pch.h"

#include "data_channel.h"
#include "external_video_track_source.h"
#include "local_video_track.h"
#include "peer_connection.h"

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

class RemoteDescriptionAppliedObserver {
 public:
  static void MRS_CALL StaticExec(void* user_data) noexcept {
    auto ptr = static_cast<RemoteDescriptionAppliedObserver*>(user_data);
    try {
      ptr->Exec();
    } catch (...) {
    }
  }
  std::future<void> GetFuture() { return promise_.get_future(); }

 protected:
  void Exec() { promise_.set_value(); }
  std::promise<void> promise_;
};

const char* SdpTypeToString(SdpType type) noexcept {
  static const char* s_types[2] = {"offer", "answer"};
  return s_types[(int)type];
}

SdpType SdpTypeFromString(std::string_view str) {
  if (str == "offer") {
    return SdpType::kOffer;
  } else if (str == "answer") {
    return SdpType::kAnswer;
  }
  throw new std::invalid_argument("Unknow SDP type string value.");
}

}  // namespace

namespace Microsoft::MixedReality::WebRTC {

//
///// Implementation of PeerConnection, which also implements
///// PeerConnectionObserver at the same time to simplify interaction with
///// the underlying implementation object.
// class PeerConnection {
// public:
//  PeerConnection(mrsPeerConnectionInteropHandle interop_handle)
//      : interop_handle_(interop_handle) {
//    remote_video_observer_.reset(new VideoFrameObserver());
//    local_audio_observer_.reset(new AudioFrameObserver());
//    remote_audio_observer_.reset(new AudioFrameObserver());
//  }
//
//  ~PeerConnection() noexcept { Close(); }
//
//  void SetName(std::string_view name) { name_ = name; }
//
//  std::string GetName() const override { return name_; }
//
//  void RegisterLocalSdpReadytoSendCallback(
//      LocalSdpReadytoSendCallback&& callback) noexcept override {
//    auto lock = std::scoped_lock{local_sdp_ready_to_send_callback_mutex_};
//    local_sdp_ready_to_send_callback_ = std::move(callback);
//  }
//
//  void RegisterIceCandidateReadytoSendCallback(
//      IceCandidateReadytoSendCallback&& callback) noexcept override {
//    auto lock = std::scoped_lock{ice_candidate_ready_to_send_callback_mutex_};
//    ice_candidate_ready_to_send_callback_ = std::move(callback);
//  }
//
//  void RegisterIceStateChangedCallback(
//      IceStateChangedCallback&& callback) noexcept override {
//    auto lock = std::scoped_lock{ice_state_changed_callback_mutex_};
//    ice_state_changed_callback_ = std::move(callback);
//  }
//
//  void RegisterIceGatheringStateChangedCallback(
//      IceGatheringStateChangedCallback&& callback) noexcept override {
//    auto lock = std::scoped_lock{ice_gathering_state_changed_callback_mutex_};
//    ice_gathering_state_changed_callback_ = std::move(callback);
//  }
//
//  void RegisterRenegotiationNeededCallback(
//      RenegotiationNeededCallback&& callback) noexcept override {
//    auto lock = std::scoped_lock{renegotiation_needed_callback_mutex_};
//    renegotiation_needed_callback_ = std::move(callback);
//  }
//
//  bool AddIceCandidate(const char* sdp_mid,
//                       const int sdp_mline_index,
//                       const char* candidate) noexcept override;
//  bool SetRemoteDescription(const char* type,
//                            const char* sdp) noexcept override;
//
//  void RegisterConnectedCallback(
//      ConnectedCallback&& callback) noexcept override {
//    auto lock = std::scoped_lock{connected_callback_mutex_};
//    connected_callback_ = std::move(callback);
//  }
//
//  mrsResult SetBitrate(const BitrateSettings& settings) noexcept override {
//    webrtc::BitrateSettings bitrate;
//    bitrate.start_bitrate_bps = settings.start_bitrate_bps;
//    bitrate.min_bitrate_bps = settings.min_bitrate_bps;
//    bitrate.max_bitrate_bps = settings.max_bitrate_bps;
//    return ResultFromRTCErrorType(peer_->SetBitrate(bitrate).type());
//  }
//
//  bool CreateOffer() noexcept override;
//  bool CreateAnswer() noexcept override;
//  void Close() noexcept override;
//  bool IsClosed() const noexcept override;
//
//  void RegisterTrackAddedCallback(
//      TrackAddedCallback&& callback) noexcept override {
//    auto lock = std::scoped_lock{track_added_callback_mutex_};
//    track_added_callback_ = std::move(callback);
//  }
//
//  void RegisterTrackRemovedCallback(
//      TrackRemovedCallback&& callback) noexcept override {
//    auto lock = std::scoped_lock{track_removed_callback_mutex_};
//    track_removed_callback_ = std::move(callback);
//  }
//
//  void RegisterRemoteVideoFrameCallback(
//      I420AFrameReadyCallback callback) noexcept override {
//    if (remote_video_observer_) {
//      remote_video_observer_->SetCallback(std::move(callback));
//    }
//  }
//
//  void RegisterRemoteVideoFrameCallback(
//      Argb32FrameReadyCallback callback) noexcept override {
//    if (remote_video_observer_) {
//      remote_video_observer_->SetCallback(std::move(callback));
//    }
//  }
//
//  ErrorOr<std::shared_ptr<LocalVideoTrack>> AddLocalVideoTrack(
//      rtc::scoped_refptr<webrtc::VideoTrackInterface>
//          video_track) noexcept override;
//  webrtc::RTCError RemoveLocalVideoTrack(
//      LocalVideoTrack& video_track) noexcept override;
//  void RemoveLocalVideoTracksFromSource(
//      ExternalVideoTrackSource& source) noexcept override;
//
//  void RegisterLocalAudioFrameCallback(
//      AudioFrameReadyCallback callback) noexcept override {
//    if (local_audio_observer_) {
//      local_audio_observer_->SetCallback(std::move(callback));
//    }
//  }
//
//  void RegisterRemoteAudioFrameCallback(
//      AudioFrameReadyCallback callback) noexcept override {
//    if (remote_audio_observer_) {
//      remote_audio_observer_->SetCallback(std::move(callback));
//    }
//  }
//
//  bool AddLocalAudioTrack(rtc::scoped_refptr<webrtc::AudioTrackInterface>
//                              audio_track) noexcept override;
//  void RemoveLocalAudioTrack() noexcept override;
//  void SetLocalAudioTrackEnabled(bool enabled = true) noexcept override;
//  bool IsLocalAudioTrackEnabled() const noexcept override;
//
//  void RegisterDataChannelAddedCallback(
//      DataChannelAddedCallback callback) noexcept override {
//    auto lock = std::scoped_lock{data_channel_added_callback_mutex_};
//    data_channel_added_callback_ = std::move(callback);
//  }
//
//  void RegisterDataChannelRemovedCallback(
//      DataChannelRemovedCallback callback) noexcept override {
//    auto lock = std::scoped_lock{data_channel_removed_callback_mutex_};
//    data_channel_removed_callback_ = std::move(callback);
//  }
//
//  ErrorOr<std::shared_ptr<DataChannel>> AddDataChannel(
//      int id,
//      std::string_view label,
//      bool ordered,
//      bool reliable,
//      mrsDataChannelInteropHandle dataChannelInteropHandle) noexcept override;
//  void RemoveDataChannel(const DataChannel& data_channel) noexcept override;
//  void RemoveAllDataChannels() noexcept override;
//  void OnDataChannelAdded(const DataChannel& data_channel) noexcept override;
//
//  mrsResult RegisterInteropCallbacks(
//      const mrsPeerConnectionInteropCallbacks& callbacks) noexcept override {
//    // Make a full copy of all callbacks
//    interop_callbacks_ = callbacks;
//    return Result::kSuccess;
//  }
//
// protected:
//  /// Peer connection name assigned by the user. This has no meaning for the
//  /// implementation.
//  std::string name_;
//
//  /// Handle to the interop wrapper associated with this object.
//  mrsPeerConnectionInteropHandle interop_handle_;
//
//  /// Callbacks used for interop management.
//  mrsPeerConnectionInteropCallbacks interop_callbacks_{};
//
//  /// User callback invoked when the peer connection received a new data
//  channel
//  /// from the remote peer and added it locally.
//  DataChannelAddedCallback data_channel_added_callback_
//      RTC_GUARDED_BY(data_channel_added_callback_mutex_);
//
//  /// User callback invoked when the peer connection received a data channel
//  /// remove message from the remote peer and removed it locally.
//  DataChannelAddedCallback data_channel_removed_callback_
//      RTC_GUARDED_BY(data_channel_removed_callback_mutex_);
//
//  /// User callback invoked when the peer connection is established.
//  /// This is generally invoked even if ICE didn't finish.
//  ConnectedCallback connected_callback_
//      RTC_GUARDED_BY(connected_callback_mutex_);
//
//  /// User callback invoked when a local SDP message has been crafted by the
//  /// core engine and is ready to be sent by the signaling solution.
//  LocalSdpReadytoSendCallback local_sdp_ready_to_send_callback_
//      RTC_GUARDED_BY(local_sdp_ready_to_send_callback_mutex_);
//
//  /// User callback invoked when a local ICE message has been crafted by the
//  /// core engine and is ready to be sent by the signaling solution.
//  IceCandidateReadytoSendCallback ice_candidate_ready_to_send_callback_
//      RTC_GUARDED_BY(ice_candidate_ready_to_send_callback_mutex_);
//
//  /// User callback invoked when the ICE connection state changed.
//  IceStateChangedCallback ice_state_changed_callback_
//      RTC_GUARDED_BY(ice_state_changed_callback_mutex_);
//
//  /// User callback invoked when the ICE gathering state changed.
//  IceGatheringStateChangedCallback ice_gathering_state_changed_callback_
//      RTC_GUARDED_BY(ice_gathering_state_changed_callback_mutex_);
//
//  /// User callback invoked when SDP renegotiation is needed.
//  RenegotiationNeededCallback renegotiation_needed_callback_
//      RTC_GUARDED_BY(renegotiation_needed_callback_mutex_);
//
//  /// User callback invoked when a remote audio or video track is added.
//  TrackAddedCallback track_added_callback_
//      RTC_GUARDED_BY(track_added_callback_mutex_);
//
//  /// User callback invoked when a remote audio or video track is removed.
//  TrackRemovedCallback track_removed_callback_
//      RTC_GUARDED_BY(track_removed_callback_mutex_);
//
//  std::mutex data_channel_added_callback_mutex_;
//  std::mutex data_channel_removed_callback_mutex_;
//  std::mutex connected_callback_mutex_;
//  std::mutex local_sdp_ready_to_send_callback_mutex_;
//  std::mutex ice_candidate_ready_to_send_callback_mutex_;
//  std::mutex ice_state_changed_callback_mutex_;
//  std::mutex ice_gathering_state_changed_callback_mutex_;
//  std::mutex renegotiation_needed_callback_mutex_;
//  std::mutex track_added_callback_mutex_;
//  std::mutex track_removed_callback_mutex_;
//
//  rtc::scoped_refptr<webrtc::AudioTrackInterface> local_audio_track_;
//  rtc::scoped_refptr<webrtc::RtpSenderInterface> local_audio_sender_;
//  std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
//  remote_streams_;
//
//  /// Collection of all local video tracks associated with this peer
//  connection. std::vector<std::shared_ptr<LocalVideoTrack>>
//  local_video_tracks_
//      RTC_GUARDED_BY(tracks_mutex_);
//
//  /// Mutex for all collections of all tracks.
//  rtc::CriticalSection tracks_mutex_;
//
//  /// Collection of all data channels associated with this peer connection.
//  std::vector<std::shared_ptr<DataChannel>> data_channels_
//      RTC_GUARDED_BY(data_channel_mutex_);
//
//  /// Collection of data channels from their unique ID.
//  /// This contains only data channels pre-negotiated or opened by the remote
//  /// peer, as data channels opened locally won't have immediately a unique
//  ID. std::unordered_map<int, std::shared_ptr<DataChannel>>
//  data_channel_from_id_
//      RTC_GUARDED_BY(data_channel_mutex_);
//
//  /// Collection of data channels from their label.
//  /// This contains only data channels with a non-empty label.
//  std::unordered_multimap<str, std::shared_ptr<DataChannel>>
//      data_channel_from_label_ RTC_GUARDED_BY(data_channel_mutex_);
//
//  /// Mutex for data structures related to data channels.
//  std::mutex data_channel_mutex_;
//
//  //< TODO - Clarify lifetime of those, for now same as this PeerConnection
//  std::unique_ptr<AudioFrameObserver> local_audio_observer_;
//  std::unique_ptr<AudioFrameObserver> remote_audio_observer_;
//  std::unique_ptr<VideoFrameObserver> remote_video_observer_;
//
// private:
//  PeerConnection(const PeerConnection&) = delete;
//  PeerConnection& operator=(const PeerConnection&) = delete;
//};

namespace {

const std::string kAudioVideoStreamId("local_av_stream");

/// Temporary helper to ensure a string is null-terminated before being passed
/// as an argument to the interop API. The string is only valid for the lifetime
/// of the object, and may use a view over the argument it is constructed from,
/// so should only be used as a temporary:
///   std::string_view view = ...;
///   CallMe(CString(view));
template <int N = 128>
class CString {
 public:
  CString(std::string_view view) {
    // Empty string uses in-place buffer
    if (view.empty()) {
      buffer_[0] = '\0';
      ptr_ = buffer_;
      return;
    }
    // Null-terminated view is used directly
    const size_t len = view.length();
    if (view[len - 1] == '\0') {
      ptr_ = view.data();
      return;
    }
    // Small string is copied to in-place buffer
    if (len < N) {
      memcpy(buffer_, view.data(), len);
      buffer_[len] = '\0';
      ptr_ = buffer_;
      return;
    }
    // Fallback: allocate
    data_.resize(len + 1);
    memcpy(data_.data(), view.data(), len);
    data_[len] = '\0';
    ptr_ = data_.data();
  }
  const char* c_str() const noexcept { return ptr_; }
  operator const char*() const noexcept { return ptr_; }

 protected:
  const char* ptr_{};
  std::vector<char> data_;
  char buffer_[N];
};

}  // namespace

ErrorOr<std::shared_ptr<LocalVideoTrack>> PeerConnection::AddLocalVideoTrack(
    std::string_view track_name,
    const VideoDeviceConfiguration& config) {
  LocalVideoTrackHandle track_interop_handle{};
  CheckResult(mrsPeerConnectionAddLocalVideoTrack(
      GetHandle(), CString(track_name), config, &track_interop_handle));
  return std::make_shared<LocalVideoTrack>(*this, track_interop_handle);
}

void PeerConnection::RemoveLocalVideoTrack(LocalVideoTrack& video_track) {
  CheckResult(mrsPeerConnectionRemoveLocalVideoTrack(
      GetHandle(), video_track.GetHandle()));
}

void PeerConnection::RemoveLocalVideoTracksFromSource(
    ExternalVideoTrackSource& source) {
  CheckResult(mrsPeerConnectionRemoveLocalVideoTracksFromSource(
      GetHandle(), source.GetHandle()));
}

void PeerConnection::SetLocalAudioTrackEnabled(bool enabled) {
  CheckResult(mrsPeerConnectionSetLocalAudioTrackEnabled(
      GetHandle(), enabled ? mrsBool::kTrue : mrsBool::kFalse));
}

bool PeerConnection::IsLocalAudioTrackEnabled() const noexcept {
  return (mrsPeerConnectionIsLocalAudioTrackEnabled(GetHandle()) !=
          mrsBool::kFalse);
}

void PeerConnection::AddLocalAudioTrack() {
  CheckResult(mrsPeerConnectionAddLocalAudioTrack(GetHandle()));
}

void PeerConnection::RemoveLocalAudioTrack() {
  mrsPeerConnectionRemoveLocalAudioTrack(GetHandle());
}

std::shared_ptr<DataChannel> PeerConnection::AddDataChannel(
    int id,
    std::string_view label,
    bool ordered,
    bool reliable) {
  // Create the wrapper
  std::unique_ptr<DataChannel> data_channel{
      new DataChannel(this, id, label, ordered, reliable)};

  // Setup static trampolines
  mrsDataChannelCallbacks callbacks{};
  callbacks.message_callback = &DataChannel::StaticMessageCallback;
  callbacks.message_user_data = this;
  callbacks.buffering_callback = &DataChannel::StaticBufferingCallback;
  callbacks.buffering_user_data = this;
  callbacks.state_callback = &DataChannel::StaticStateCallback;
  callbacks.state_user_data = this;

  // Create the data channel implementation and add it to the peer connection
  mrsDataChannelConfig config{};
  config.id = id;
  config.label = CString(label);
  if (ordered) {
    config.flags |= mrsDataChannelConfigFlags::kOrdered;
  }
  if (reliable) {
    config.flags |= mrsDataChannelConfigFlags::kReliable;
  }
  DataChannelHandle handle{};
  mrsDataChannelInteropHandle interop_handle = this;
  CheckResult(mrsPeerConnectionAddDataChannel(GetHandle(), interop_handle,
                                              config, callbacks, &handle));

  // On success, assign the handle to the newly created wrapper and return it
  data_channel->handle_ = handle;
  return data_channel;
}

void PeerConnection::RemoveDataChannel(const DataChannel& data_channel) {
  CheckResult(mrsPeerConnectionRemoveDataChannel(GetHandle(),
                                                 data_channel.GetHandle()));
}

void PeerConnection::RemoveAllDataChannels() {
  //< FIXME...
  throw new NotImplementedException();
  // CheckResult(mrsPeerConnectionRemoveAllDataChannels(GetHandle()));
}

void PeerConnection::AddIceCandidate(const IceCandidate& candidate) {
  CheckResult(mrsPeerConnectionAddIceCandidate(
      GetHandle(), candidate.sdp_mid.c_str(), candidate.sdp_mline_index,
      candidate.candidate.c_str()));
}

std::future<SdpDescription> PeerConnection::CreateOfferAsync() {
  CheckResult(mrsPeerConnectionCreateOffer(GetHandle()));
  //< FIXME...
  throw new NotImplementedException();
}

std::future<SdpDescription> PeerConnection::CreateAnswerAsync() {
  CheckResult(mrsPeerConnectionCreateAnswer(GetHandle()));
  //< FIXME...
  throw new NotImplementedException();
}

void PeerConnection::Close() {
  if (!handle_) {
    return;
  }
  CheckResult(mrsPeerConnectionClose(handle_));
  handle_ = nullptr;
}

std::future<void> PeerConnection::SetRemoteDescriptionAsync(
    const SdpDescription& desc) {
  auto observer = std::make_unique<RemoteDescriptionAppliedObserver>();
  CheckResult(mrsPeerConnectionSetRemoteDescription(
      GetHandle(), SdpTypeToString(desc.type), desc.sdp.c_str(),
      &RemoteDescriptionAppliedObserver::StaticExec, observer.get()));
  observer.release();  // no exception, keep alive
  return observer->GetFuture();
}

}  // namespace Microsoft::MixedReality::WebRTC

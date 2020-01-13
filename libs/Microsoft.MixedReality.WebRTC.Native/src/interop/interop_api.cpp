// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// This is a precompiled header, it must be on its own, followed by a blank
// line, to prevent clang-format from reordering it with other headers.
#include "pch.h"

#include "data_channel.h"
#include "external_video_track_source.h"
#include "interop/external_video_track_source_interop.h"
#include "interop/global_factory.h"
#include "interop/interop_api.h"
#include "interop/local_audio_track_interop.h"
#include "interop/local_video_track_interop.h"
#include "interop/peer_connection_interop.h"
#include "local_video_track.h"
#include "media/external_video_track_source_impl.h"
#include "media/local_audio_track.h"
#include "peer_connection.h"
#include "sdp_utils.h"

using namespace Microsoft::MixedReality::WebRTC;

struct mrsEnumerator {
  virtual ~mrsEnumerator() = default;
  virtual void dispose() = 0;
};

namespace {

inline bool IsStringNullOrEmpty(const char* str) noexcept {
  return ((str == nullptr) || (str[0] == '\0'));
}

mrsResult RTCToAPIError(const webrtc::RTCError& error) {
  if (error.ok()) {
    return Result::kSuccess;
  }
  switch (error.type()) {
    case webrtc::RTCErrorType::INVALID_PARAMETER:
    case webrtc::RTCErrorType::INVALID_RANGE:
      return Result::kInvalidParameter;
    case webrtc::RTCErrorType::INVALID_STATE:
      return Result::kInvalidOperation;
    case webrtc::RTCErrorType::INTERNAL_ERROR:
    default:
      return Result::kUnknownError;
  }
}

#if defined(WINUWP)
using WebRtcFactoryPtr =
    std::shared_ptr<wrapper::impl::org::webRtc::WebRtcFactory>;
#endif  // defined(WINUWP)

/// Predefined name of the local audio track.
const std::string kLocalAudioLabel("local_audio");

class SimpleMediaConstraints : public webrtc::MediaConstraintsInterface {
 public:
  using webrtc::MediaConstraintsInterface::Constraint;
  using webrtc::MediaConstraintsInterface::Constraints;
  static Constraint MinWidth(uint32_t min_width) {
    return Constraint(webrtc::MediaConstraintsInterface::kMinWidth,
                      std::to_string(min_width));
  }
  static Constraint MaxWidth(uint32_t max_width) {
    return Constraint(webrtc::MediaConstraintsInterface::kMaxWidth,
                      std::to_string(max_width));
  }
  static Constraint MinHeight(uint32_t min_height) {
    return Constraint(webrtc::MediaConstraintsInterface::kMinHeight,
                      std::to_string(min_height));
  }
  static Constraint MaxHeight(uint32_t max_height) {
    return Constraint(webrtc::MediaConstraintsInterface::kMaxHeight,
                      std::to_string(max_height));
  }
  static Constraint MinFrameRate(double min_framerate) {
    // Note: kMinFrameRate is read back as an int
    const int min_int = (int)std::floor(min_framerate);
    return Constraint(webrtc::MediaConstraintsInterface::kMinFrameRate,
                      std::to_string(min_int));
  }
  static Constraint MaxFrameRate(double max_framerate) {
    // Note: kMinFrameRate is read back as an int
    const int max_int = (int)std::ceil(max_framerate);
    return Constraint(webrtc::MediaConstraintsInterface::kMaxFrameRate,
                      std::to_string(max_int));
  }
  const Constraints& GetMandatory() const override { return mandatory_; }
  const Constraints& GetOptional() const override { return optional_; }
  Constraints mandatory_;
  Constraints optional_;
};

/// Helper to open a video capture device.
mrsResult OpenVideoCaptureDevice(
    const LocalVideoTrackInitConfig& config,
    std::unique_ptr<cricket::VideoCapturer>& capturer_out) noexcept {
  capturer_out.reset();
#if defined(WINUWP)
  WebRtcFactoryPtr uwp_factory;
  {
    mrsResult res =
        GlobalFactory::Instance()->GetOrCreateWebRtcFactory(uwp_factory);
    if (res != Result::kSuccess) {
      RTC_LOG(LS_ERROR) << "Failed to initialize the UWP factory.";
      return res;
    }
  }

  // Check for calls from main UI thread; this is not supported (will deadlock)
  auto mw = winrt::Windows::ApplicationModel::Core::CoreApplication::MainView();
  auto cw = mw.CoreWindow();
  auto dispatcher = cw.Dispatcher();
  if (dispatcher.HasThreadAccess()) {
    return Result::kWrongThread;
  }

  // Get devices synchronously (wait for UI thread to retrieve them for us)
  rtc::Event blockOnDevicesEvent(true, false);
  auto vci = wrapper::impl::org::webRtc::VideoCapturer::getDevices();
  vci->thenClosure([&blockOnDevicesEvent] { blockOnDevicesEvent.Set(); });
  blockOnDevicesEvent.Wait(rtc::Event::kForever);
  auto deviceList = vci->value();

  std::wstring video_device_id_str;
  if (!IsStringNullOrEmpty(config.video_device_id)) {
    video_device_id_str =
        rtc::ToUtf16(config.video_device_id, strlen(config.video_device_id));
  }

  for (auto&& vdi : *deviceList) {
    auto devInfo =
        wrapper::impl::org::webRtc::VideoDeviceInfo::toNative_winrt(vdi);
    const winrt::hstring& id = devInfo.Id();
    if (!video_device_id_str.empty() && (video_device_id_str != id)) {
      RTC_LOG(LS_VERBOSE) << "Skipping device ID " << rtc::ToUtf8(id.c_str())
                          << " not matching requested device.";
      continue;
    }

    auto createParams =
        wrapper::org::webRtc::VideoCapturerCreationParameters::wrapper_create();
    createParams->factory = uwp_factory;
    createParams->name = devInfo.Name().c_str();
    createParams->id = id.c_str();
    if (config.video_profile_id) {
      createParams->videoProfileId = config.video_profile_id;
    }
    createParams->videoProfileKind =
        (wrapper::org::webRtc::VideoProfileKind)config.video_profile_kind;
    createParams->enableMrc = (config.enable_mrc != mrsBool::kFalse);
    createParams->enableMrcRecordingIndicator =
        (config.enable_mrc_recording_indicator != mrsBool::kFalse);
    createParams->width = config.width;
    createParams->height = config.height;
    createParams->framerate = config.framerate;

    auto vcd = wrapper::impl::org::webRtc::VideoCapturer::create(createParams);
    if (vcd != nullptr) {
      auto nativeVcd = wrapper::impl::org::webRtc::VideoCapturer::toNative(vcd);

      RTC_LOG(LS_INFO) << "Using video capture device '"
                       << createParams->name.c_str()
                       << "' (id=" << createParams->id.c_str() << ")";

      if (auto supportedFormats = nativeVcd->GetSupportedFormats()) {
        RTC_LOG(LS_INFO) << "Supported video formats:";
        for (auto&& format : *supportedFormats) {
          auto str = format.ToString();
          RTC_LOG(LS_INFO) << "- " << str.c_str();
        }
      }

      capturer_out = std::move(nativeVcd);
      return Result::kSuccess;
    }
  }
  RTC_LOG(LS_ERROR) << "Failed to find a local video capture device matching "
                       "the capture format constraints. None of the "
                    << deviceList->size()
                    << " devices tested had a compatible capture format.";
  return Result::kNotFound;
#else
  // List all available video capture devices, or match by ID if specified.
  std::vector<std::string> device_names;
  {
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
        webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info) {
      return Result::kUnknownError;
    }

    const int num_devices = info->NumberOfDevices();
    constexpr uint32_t kSize = 256;
    if (!IsStringNullOrEmpty(config.video_device_id)) {
      // Look for the one specific device the user asked for
      std::string video_device_id_str = config.video_device_id;
      for (int i = 0; i < num_devices; ++i) {
        char name[kSize] = {};
        char id[kSize] = {};
        if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) {
          if (video_device_id_str == id) {
            // Keep only the device the user selected
            device_names.push_back(name);
            break;
          }
        }
      }
      if (device_names.empty()) {
        RTC_LOG(LS_ERROR)
            << "Could not find video capture device by unique ID: "
            << config.video_device_id;
        return Result::kNotFound;
      }
    } else {
      // List all available devices
      for (int i = 0; i < num_devices; ++i) {
        char name[kSize] = {};
        char id[kSize] = {};
        if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) {
          device_names.push_back(name);
        }
      }
      if (device_names.empty()) {
        RTC_LOG(LS_ERROR) << "Could not find any video catpure device.";
        return Result::kNotFound;
      }
    }
  }

  // Open the specified capture device, or the first one available if none
  // specified.
  cricket::WebRtcVideoDeviceCapturerFactory factory;
  for (const auto& name : device_names) {
    // cricket::Device identifies devices by (friendly) name, not unique ID
    capturer_out = factory.Create(cricket::Device(name, 0));
    if (capturer_out) {
      return Result::kSuccess;
    }
  }
  RTC_LOG(LS_ERROR) << "Failed to open any video capture device (tried "
                    << device_names.size() << " devices).";
  return Result::kUnknownError;
#endif
}

//< TODO - Unit test / check if RTC has already a utility like this
std::vector<std::string> SplitString(const std::string& str, char sep) {
  std::vector<std::string> ret;
  size_t offset = 0;
  for (size_t idx = str.find_first_of(sep); idx < std::string::npos;
       idx = str.find_first_of(sep, offset)) {
    if (idx > offset) {
      ret.push_back(str.substr(offset, idx - offset));
    }
    offset = idx + 1;
  }
  if (offset < str.size()) {
    ret.push_back(str.substr(offset));
  }
  return ret;
}

/// Convert a WebRTC VideoType format into its FOURCC counterpart.
uint32_t FourCCFromVideoType(webrtc::VideoType videoType) {
  switch (videoType) {
    default:
    case webrtc::VideoType::kUnknown:
      return (uint32_t)libyuv::FOURCC_ANY;
    case webrtc::VideoType::kI420:
      return (uint32_t)libyuv::FOURCC_I420;
    case webrtc::VideoType::kIYUV:
      return (uint32_t)libyuv::FOURCC_IYUV;
    case webrtc::VideoType::kRGB24:
      // this seems unintuitive, but is how defined in the core implementation
      return (uint32_t)libyuv::FOURCC_24BG;
    case webrtc::VideoType::kABGR:
      return (uint32_t)libyuv::FOURCC_ABGR;
    case webrtc::VideoType::kARGB:
      return (uint32_t)libyuv::FOURCC_ARGB;
    case webrtc::VideoType::kARGB4444:
      return (uint32_t)libyuv::FOURCC_R444;
    case webrtc::VideoType::kRGB565:
      return (uint32_t)libyuv::FOURCC_RGBP;
    case webrtc::VideoType::kARGB1555:
      return (uint32_t)libyuv::FOURCC_RGBO;
    case webrtc::VideoType::kYUY2:
      return (uint32_t)libyuv::FOURCC_YUY2;
    case webrtc::VideoType::kYV12:
      return (uint32_t)libyuv::FOURCC_YV12;
    case webrtc::VideoType::kUYVY:
      return (uint32_t)libyuv::FOURCC_UYVY;
    case webrtc::VideoType::kMJPEG:
      return (uint32_t)libyuv::FOURCC_MJPG;
    case webrtc::VideoType::kNV21:
      return (uint32_t)libyuv::FOURCC_NV21;
    case webrtc::VideoType::kNV12:
      return (uint32_t)libyuv::FOURCC_NV12;
    case webrtc::VideoType::kBGRA:
      return (uint32_t)libyuv::FOURCC_BGRA;
  };
}

}  // namespace

inline rtc::Thread* GetWorkerThread() {
  return GlobalFactory::Instance()->GetWorkerThread();
}

void MRS_CALL mrsCloseEnum(mrsEnumHandle* handleRef) noexcept {
  if (handleRef) {
    if (auto& handle = *handleRef) {
      handle->dispose();
      delete handle;
      handle = nullptr;
    }
  }
}

mrsResult MRS_CALL mrsEnumVideoCaptureDevicesAsync(
    mrsVideoCaptureDeviceEnumCallback enumCallback,
    void* enumCallbackUserData,
    mrsVideoCaptureDeviceEnumCompletedCallback completedCallback,
    void* completedCallbackUserData) noexcept {
  if (!enumCallback) {
    return Result::kInvalidParameter;
  }
#if defined(WINUWP)
  // The UWP factory needs to be initialized for getDevices() to work.
  if (!GlobalFactory::Instance()->GetOrCreate()) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the UWP factory.";
    return Result::kUnknownError;
  }

  auto vci = wrapper::impl::org::webRtc::VideoCapturer::getDevices();
  vci->thenClosure([vci, enumCallback, completedCallback, enumCallbackUserData,
                    completedCallbackUserData] {
    auto deviceList = vci->value();
    for (auto&& vdi : *deviceList) {
      auto devInfo =
          wrapper::impl::org::webRtc::VideoDeviceInfo::toNative_winrt(vdi);
      auto id = winrt::to_string(devInfo.Id());
      auto name = winrt::to_string(devInfo.Name());
      (*enumCallback)(id.c_str(), name.c_str(), enumCallbackUserData);
    }
    if (completedCallback) {
      (*completedCallback)(completedCallbackUserData);
    }
  });
  return Result::kSuccess;
#else
  std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());
  if (!info) {
    RTC_LOG(LS_ERROR) << "Failed to start video capture devices enumeration.";
    if (completedCallback) {
      (*completedCallback)(completedCallbackUserData);
    }
    return Result::kUnknownError;
  }
  int num_devices = info->NumberOfDevices();
  for (int i = 0; i < num_devices; ++i) {
    constexpr uint32_t kSize = 256;
    char name[kSize] = {0};
    char id[kSize] = {0};
    if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) {
      (*enumCallback)(id, name, enumCallbackUserData);
    }
  }
  if (completedCallback) {
    (*completedCallback)(completedCallbackUserData);
  }
  return Result::kSuccess;
#endif
}

mrsResult MRS_CALL mrsEnumVideoCaptureFormatsAsync(
    const char* device_id,
    mrsVideoCaptureFormatEnumCallback enumCallback,
    void* enumCallbackUserData,
    mrsVideoCaptureFormatEnumCompletedCallback completedCallback,
    void* completedCallbackUserData) noexcept {
  if (IsStringNullOrEmpty(device_id)) {
    return Result::kInvalidParameter;
  }
  const std::string device_id_str = device_id;

  if (!enumCallback) {
    return Result::kInvalidParameter;
  }

#if defined(WINUWP)
  // The UWP factory needs to be initialized for getDevices() to work.
  WebRtcFactoryPtr uwp_factory;
  {
    mrsResult res =
        GlobalFactory::Instance()->GetOrCreateWebRtcFactory(uwp_factory);
    if (res != Result::kSuccess) {
      RTC_LOG(LS_ERROR) << "Failed to initialize the UWP factory.";
      return res;
    }
  }

  // On UWP, MediaCapture is used to open the video capture device and list
  // the available capture formats. This requires the UI thread to be idle,
  // ready to process messages. Because the enumeration is async, and this
  // function can return before the enumeration completed, if called on the
  // main UI thread then defer all of it to a different thread.
  // auto mw =
  // winrt::Windows::ApplicationModel::Core::CoreApplication::MainView(); auto
  // cw = mw.CoreWindow(); auto dispatcher = cw.Dispatcher(); if
  // (dispatcher.HasThreadAccess()) {
  //  if (completedCallback) {
  //    (*completedCallback)(Result::kWrongThread,
  //    completedCallbackUserData);
  //  }
  //  return Result::kWrongThread;
  //}

  // Enumerate the video capture devices
  auto asyncResults =
      winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
          winrt::Windows::Devices::Enumeration::DeviceClass::VideoCapture);
  asyncResults.Completed([device_id_str, enumCallback, completedCallback,
                          enumCallbackUserData, completedCallbackUserData,
                          uwp_factory = std::move(uwp_factory)](
                             auto&& asyncResults,
                             winrt::Windows::Foundation::AsyncStatus status) {
    // If the OS enumeration failed, terminate our own enumeration
    if (status != winrt::Windows::Foundation::AsyncStatus::Completed) {
      if (completedCallback) {
        (*completedCallback)(Result::kUnknownError, completedCallbackUserData);
      }
      return;
    }
    winrt::Windows::Devices::Enumeration::DeviceInformationCollection
        devInfoCollection = asyncResults.GetResults();

    // Find the video capture device by unique identifier
    winrt::Windows::Devices::Enumeration::DeviceInformation devInfo(nullptr);
    for (auto curDevInfo : devInfoCollection) {
      auto id = winrt::to_string(curDevInfo.Id());
      if (id != device_id_str) {
        continue;
      }
      devInfo = curDevInfo;
      break;
    }
    if (!devInfo) {
      if (completedCallback) {
        (*completedCallback)(Result::kInvalidParameter,
                             completedCallbackUserData);
      }
      return;
    }

    // Device found, create an instance to enumerate. Most devices require
    // actually opening the device to enumerate its capture formats.
    auto createParams =
        wrapper::org::webRtc::VideoCapturerCreationParameters::wrapper_create();
    createParams->factory = uwp_factory;
    createParams->name = devInfo.Name().c_str();
    createParams->id = devInfo.Id().c_str();
    auto vcd = wrapper::impl::org::webRtc::VideoCapturer::create(createParams);
    if (vcd == nullptr) {
      if (completedCallback) {
        (*completedCallback)(Result::kUnknownError, completedCallbackUserData);
      }
      return;
    }

    // Get its supported capture formats
    auto captureFormatList = vcd->getSupportedFormats();
    for (auto&& captureFormat : *captureFormatList) {
      uint32_t width = captureFormat->get_width();
      uint32_t height = captureFormat->get_height();
      double framerate = captureFormat->get_framerateFloat();
      uint32_t fourcc = captureFormat->get_fourcc();

      // When VideoEncodingProperties.Subtype() contains a GUID, the
      // conversion to FOURCC fails and returns FOURCC_ANY. So ignore
      // those formats, as we don't know their encoding.
      if (fourcc != libyuv::FOURCC_ANY) {
        (*enumCallback)(width, height, framerate, fourcc, enumCallbackUserData);
      }
    }

    // Invoke the completed callback at the end of enumeration
    if (completedCallback) {
      (*completedCallback)(Result::kSuccess, completedCallbackUserData);
    }
  });
#else   // defined(WINUWP)
  std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());
  if (!info) {
    return Result::kUnknownError;
  }
  int num_devices = info->NumberOfDevices();
  for (int device_idx = 0; device_idx < num_devices; ++device_idx) {
    // Filter devices by name
    constexpr uint32_t kSize = 256;
    char name[kSize] = {0};
    char id[kSize] = {0};
    if (info->GetDeviceName(device_idx, name, kSize, id, kSize) == -1) {
      continue;
    }
    if (id != device_id_str) {
      continue;
    }

    // Enum video capture formats
    int32_t num_capabilities = info->NumberOfCapabilities(id);
    for (int32_t cap_idx = 0; cap_idx < num_capabilities; ++cap_idx) {
      webrtc::VideoCaptureCapability capability{};
      if (info->GetCapability(id, cap_idx, capability) != -1) {
        uint32_t width = capability.width;
        uint32_t height = capability.height;
        double framerate = capability.maxFPS;
        uint32_t fourcc = FourCCFromVideoType(capability.videoType);
        if (fourcc != libyuv::FOURCC_ANY) {
          (*enumCallback)(width, height, framerate, fourcc,
                          enumCallbackUserData);
        }
      }
    }

    break;
  }

  // Invoke the completed callback at the end of enumeration
  if (completedCallback) {
    (*completedCallback)(Result::kSuccess, completedCallbackUserData);
  }
#endif  // defined(WINUWP)

  // If the async operation was successfully queued, return successfully.
  // Note that the enumeration is asynchronous, so not done yet.
  return Result::kSuccess;
}
mrsResult MRS_CALL
mrsPeerConnectionCreate(PeerConnectionConfiguration config,
                        mrsPeerConnectionInteropHandle interop_handle,
                        PeerConnectionHandle* peerHandleOut) noexcept {
  if (!peerHandleOut || !interop_handle) {
    return Result::kInvalidParameter;
  }
  *peerHandleOut = nullptr;

  // Create the new peer connection
  auto result = PeerConnection::create(config, interop_handle);
  if (!result.ok()) {
    return result.error().result();
  }
  *peerHandleOut = (PeerConnectionHandle)result.value().release();
  return Result::kSuccess;
}

mrsResult MRS_CALL mrsPeerConnectionRegisterInteropCallbacks(
    PeerConnectionHandle peerHandle,
    mrsPeerConnectionInteropCallbacks* callbacks) noexcept {
  if (!callbacks) {
    return Result::kInvalidParameter;
  }
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    return peer->RegisterInteropCallbacks(*callbacks);
  }
  return Result::kInvalidNativeHandle;
}

void MRS_CALL mrsPeerConnectionRegisterConnectedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionConnectedCallback callback,
    void* user_data) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->RegisterConnectedCallback(Callback<>{callback, user_data});
  }
}

void MRS_CALL mrsPeerConnectionRegisterLocalSdpReadytoSendCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionLocalSdpReadytoSendCallback callback,
    void* user_data) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->RegisterLocalSdpReadytoSendCallback(
        Callback<const char*, const char*>{callback, user_data});
  }
}

void MRS_CALL mrsPeerConnectionRegisterIceCandidateReadytoSendCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionIceCandidateReadytoSendCallback callback,
    void* user_data) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->RegisterIceCandidateReadytoSendCallback(
        Callback<const char*, int, const char*>{callback, user_data});
  }
}

void MRS_CALL mrsPeerConnectionRegisterIceStateChangedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionIceStateChangedCallback callback,
    void* user_data) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->RegisterIceStateChangedCallback(
        Callback<IceConnectionState>{callback, user_data});
  }
}

void MRS_CALL mrsPeerConnectionRegisterRenegotiationNeededCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionRenegotiationNeededCallback callback,
    void* user_data) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->RegisterRenegotiationNeededCallback(Callback<>{callback, user_data});
  }
}

void MRS_CALL mrsPeerConnectionRegisterAudioTrackAddedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionAudioTrackAddedCallback callback,
    void* user_data) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->RegisterAudioTrackAddedCallback(
        Callback<mrsRemoteAudioTrackInteropHandle, RemoteAudioTrackHandle,
                 mrsAudioTransceiverInteropHandle, AudioTransceiverHandle>{
            callback, user_data});
  }
}

void MRS_CALL mrsPeerConnectionRegisterAudioTrackRemovedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionAudioTrackRemovedCallback callback,
    void* user_data) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->RegisterAudioTrackRemovedCallback(
        Callback<mrsRemoteAudioTrackInteropHandle, RemoteAudioTrackHandle,
                 mrsAudioTransceiverInteropHandle, AudioTransceiverHandle>{
            callback, user_data});
  }
}

void MRS_CALL mrsPeerConnectionRegisterVideoTrackAddedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionVideoTrackAddedCallback callback,
    void* user_data) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->RegisterVideoTrackAddedCallback(
        Callback<mrsRemoteVideoTrackInteropHandle, RemoteVideoTrackHandle,
                 mrsVideoTransceiverInteropHandle, VideoTransceiverHandle>{
            callback, user_data});
  }
}

void MRS_CALL mrsPeerConnectionRegisterVideoTrackRemovedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionVideoTrackRemovedCallback callback,
    void* user_data) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->RegisterVideoTrackRemovedCallback(
        Callback<mrsRemoteVideoTrackInteropHandle, RemoteVideoTrackHandle,
                 mrsVideoTransceiverInteropHandle, VideoTransceiverHandle>{
            callback, user_data});
  }
}

void MRS_CALL mrsPeerConnectionRegisterDataChannelAddedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionDataChannelAddedCallback callback,
    void* user_data) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->RegisterDataChannelAddedCallback(
        Callback<mrsDataChannelInteropHandle, DataChannelHandle>{callback,
                                                                 user_data});
  }
}

void MRS_CALL mrsPeerConnectionRegisterDataChannelRemovedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionDataChannelRemovedCallback callback,
    void* user_data) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->RegisterDataChannelRemovedCallback(
        Callback<mrsDataChannelInteropHandle, DataChannelHandle>{callback,
                                                                 user_data});
  }
}

mrsResult MRS_CALL
mrsLocalVideoTrackCreate(const LocalVideoTrackInitConfig* config,
                         const char* track_name,
                         LocalVideoTrackHandle* track_handle_out) noexcept {
  if (IsStringNullOrEmpty(track_name)) {
    RTC_LOG(LS_ERROR) << "Invalid empty local video track name.";
    return Result::kInvalidParameter;
  }
  if (!config) {
    RTC_LOG(LS_ERROR) << "Invalid NULL local video device configuration.";
    return Result::kInvalidParameter;
  }
  if (!track_handle_out) {
    RTC_LOG(LS_ERROR) << "Invalid NULL local video track handle.";
    return Result::kInvalidParameter;
  }
  *track_handle_out = nullptr;

  auto pc_factory = GlobalFactory::Instance()->GetExisting();
  if (!pc_factory) {
    return Result::kInvalidOperation;
  }

  // Open the video capture device
  std::unique_ptr<cricket::VideoCapturer> video_capturer;
  auto res = OpenVideoCaptureDevice(*config, video_capturer);
  if (res != Result::kSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to open video capture device.";
    return res;
  }
  RTC_CHECK(video_capturer.get());

  // Apply the same constraints used for opening the video capturer
  auto videoConstraints = std::make_unique<SimpleMediaConstraints>();
  if (config->width > 0) {
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MinWidth(config->width));
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MaxWidth(config->width));
  }
  if (config->height > 0) {
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MinHeight(config->height));
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MaxHeight(config->height));
  }
  if (config->framerate > 0) {
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MinFrameRate(config->framerate));
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MaxFrameRate(config->framerate));
  }

  // Create the video track source
  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source =
      pc_factory->CreateVideoSource(std::move(video_capturer),
                                    videoConstraints.get());
  if (!video_source) {
    return Result::kUnknownError;
  }

  // Create the video track
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      pc_factory->CreateVideoTrack(track_name, video_source);
  if (!video_track) {
    RTC_LOG(LS_ERROR) << "Failed to create local video track.";
    return Result::kUnknownError;
  }

  // Create the video track wrapper
  RefPtr<LocalVideoTrack> track =
      new LocalVideoTrack(std::move(video_track), config->track_interop_handle);
  *track_handle_out = track.release();
  return Result::kSuccess;
}

mrsResult MRS_CALL mrsPeerConnectionAddLocalVideoTrack(
    PeerConnectionHandle peer_handle,
    const char* track_name,
    const LocalVideoTrackInitConfig* config,
    LocalVideoTrackHandle* track_handle_out,
    VideoTransceiverHandle* transceiver_handle_out) noexcept {
  if (IsStringNullOrEmpty(track_name)) {
    RTC_LOG(LS_ERROR) << "Invalid empty local video track name.";
    return Result::kInvalidParameter;
  }
  if (!config) {
    RTC_LOG(LS_ERROR) << "Invalid NULL local video device configuration.";
    return Result::kInvalidParameter;
  }
  if (!track_handle_out) {
    RTC_LOG(LS_ERROR) << "Invalid NULL local video track handle.";
    return Result::kInvalidParameter;
  }
  if (!transceiver_handle_out) {
    RTC_LOG(LS_ERROR) << "Invalid NULL video transceiver handle.";
    return Result::kInvalidParameter;
  }
  *track_handle_out = nullptr;
  *transceiver_handle_out = nullptr;

  auto peer = static_cast<PeerConnection*>(peer_handle);
  if (!peer) {
    RTC_LOG(LS_ERROR) << "Invalid NULL peer connection handle.";
    return Result::kInvalidNativeHandle;
  }
  auto pc_factory = GlobalFactory::Instance()->GetExisting();
  if (!pc_factory) {
    return Result::kInvalidOperation;
  }

  // Open the video capture device
  std::unique_ptr<cricket::VideoCapturer> video_capturer;
  auto res = OpenVideoCaptureDevice(*config, video_capturer);
  if (res != Result::kSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to open video capture device.";
    return res;
  }
  RTC_CHECK(video_capturer.get());

  // Apply the same constraints used for opening the video capturer
  auto videoConstraints = std::make_unique<SimpleMediaConstraints>();
  if (config->width > 0) {
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MinWidth(config->width));
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MaxWidth(config->width));
  }
  if (config->height > 0) {
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MinHeight(config->height));
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MaxHeight(config->height));
  }
  if (config->framerate > 0) {
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MinFrameRate(config->framerate));
    videoConstraints->mandatory_.push_back(
        SimpleMediaConstraints::MaxFrameRate(config->framerate));
  }

  // Create the video track source
  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source =
      pc_factory->CreateVideoSource(std::move(video_capturer),
                                    videoConstraints.get());
  if (!video_source) {
    return Result::kUnknownError;
  }

  // Create the video track
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      pc_factory->CreateVideoTrack(track_name, video_source);
  if (!video_track) {
    RTC_LOG(LS_ERROR) << "Failed to create local video track.";
    return Result::kUnknownError;
  }

  // Add the video track to the peer connection
  auto result = peer->AddLocalVideoTrack(std::move(video_track),
                                         config->transceiver_interop_handle,
                                         config->track_interop_handle);
  if (result.ok()) {
    RefPtr<LocalVideoTrack>& video_track_wrapper = result.value();
    RefPtr<VideoTransceiver> video_transceiver =
        video_track_wrapper->GetTransceiver();
    *track_handle_out = video_track_wrapper.release();
    *transceiver_handle_out = video_transceiver.release();
    return Result::kSuccess;
  }
  RTC_LOG(LS_ERROR) << "Failed to add local video track to peer connection.";
  return Result::kUnknownError;
}

mrsResult MRS_CALL mrsPeerConnectionAddLocalVideoTrackFromExternalSource(
    PeerConnectionHandle peer_handle,
    const char* track_name,
    const LocalVideoTrackFromExternalSourceInitConfig* config,
    LocalVideoTrackHandle* track_handle_out,
    VideoTransceiverHandle* transceiver_handle_out) noexcept {
  if (!track_handle_out || !transceiver_handle_out) {
    return Result::kInvalidParameter;
  }
  *track_handle_out = nullptr;
  *transceiver_handle_out = nullptr;
  auto peer = static_cast<PeerConnection*>(peer_handle);
  if (!peer) {
    return Result::kInvalidNativeHandle;
  }
  auto track_source =
      static_cast<detail::ExternalVideoTrackSourceImpl*>(config->source_handle);
  if (!track_source) {
    return Result::kInvalidNativeHandle;
  }
  auto pc_factory = GlobalFactory::Instance()->GetExisting();
  if (!pc_factory) {
    return Result::kUnknownError;
  }
  std::string track_name_str;
  if (track_name && (track_name[0] != '\0')) {
    track_name_str = track_name;
  } else {
    track_name_str = "external_track";
  }
  // The video track keeps a reference to the video source; let's hope this
  // does not change, because this is not explicitly mentioned in the docs,
  // and the video track is the only one keeping the video source alive.
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      pc_factory->CreateVideoTrack(track_name_str, track_source->impl());
  if (!video_track) {
    return Result::kUnknownError;
  }
  auto result = peer->AddLocalVideoTrack(std::move(video_track),
                                         config->transceiver_interop_handle,
                                         config->track_interop_handle);
  if (result.ok()) {
    LocalVideoTrack* const track = result.value().release();
    RefPtr<Transceiver> transceiver = track->GetTransceiver();
    *track_handle_out = track;                        // owns the ref
    *transceiver_handle_out = transceiver.release();  // owns the ref
    return Result::kSuccess;
  }
  RTC_LOG(LS_ERROR) << "Failed to add local video track: "
                    << result.error().message();
  return Result::kUnknownError;  //< TODO Convert from result.error()?
}

mrsResult MRS_CALL mrsPeerConnectionRemoveLocalVideoTracksFromSource(
    PeerConnectionHandle peer_handle,
    ExternalVideoTrackSourceHandle source_handle) noexcept {
  auto peer = static_cast<PeerConnection*>(peer_handle);
  if (!peer) {
    return Result::kInvalidNativeHandle;
  }
  auto source = static_cast<ExternalVideoTrackSource*>(source_handle);
  if (!source) {
    return Result::kInvalidNativeHandle;
  }
  peer->RemoveLocalVideoTracksFromSource(*source);
  return Result::kSuccess;
}

mrsResult MRS_CALL
mrsLocalAudioTrackCreate(const LocalAudioTrackInitConfig* config,
                         const char* track_name,
                         LocalAudioTrackHandle* track_handle_out) noexcept {
  if (IsStringNullOrEmpty(track_name)) {
    RTC_LOG(LS_ERROR) << "Invalid empty local audio track name.";
    return Result::kInvalidParameter;
  }
  if (!track_handle_out) {
    RTC_LOG(LS_ERROR) << "Invalid NULL local audio track handle.";
    return Result::kInvalidParameter;
  }
  *track_handle_out = nullptr;

  auto pc_factory = GlobalFactory::Instance()->GetExisting();
  if (!pc_factory) {
    return Result::kInvalidOperation;
  }

  // Create the audio track source
  rtc::scoped_refptr<webrtc::AudioSourceInterface> audio_source =
      pc_factory->CreateAudioSource(cricket::AudioOptions());
  if (!audio_source) {
    RTC_LOG(LS_ERROR) << "Failed to create local audio source.";
    return Result::kUnknownError;
  }

  // Create the audio track
  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track =
      pc_factory->CreateAudioTrack(track_name, audio_source);
  if (!audio_track) {
    RTC_LOG(LS_ERROR) << "Failed to create local audio track.";
    return Result::kUnknownError;
  }

  // Create the audio track wrapper
  RefPtr<LocalAudioTrack> track =
      new LocalAudioTrack(std::move(audio_track), config->track_interop_handle);
  *track_handle_out = track.release();
  return Result::kSuccess;
}

mrsResult MRS_CALL mrsPeerConnectionAddLocalAudioTrack(
    PeerConnectionHandle peerHandle,
    const char* track_name,
    const LocalAudioTrackInitConfig* config,
    LocalAudioTrackHandle* track_handle_out,
    AudioTransceiverHandle* transceiver_handle_out) noexcept {
  if (IsStringNullOrEmpty(track_name)) {
    RTC_LOG(LS_ERROR) << "Invalid empty local audio track name.";
    return Result::kInvalidParameter;
  }
  if (!track_handle_out) {
    RTC_LOG(LS_ERROR) << "Invalid NULL local audio track handle.";
    return Result::kInvalidParameter;
  }
  *track_handle_out = nullptr;
  if (!transceiver_handle_out) {
    RTC_LOG(LS_ERROR) << "Invalid NULL audio transceiver handle.";
    return Result::kInvalidParameter;
  }
  *transceiver_handle_out = nullptr;

  auto peer = static_cast<PeerConnection*>(peerHandle);
  if (!peer) {
    RTC_LOG(LS_ERROR) << "Invalid NULL peer connection handle.";
    return Result::kInvalidNativeHandle;
  }
  auto pc_factory = GlobalFactory::Instance()->GetExisting();
  if (!pc_factory) {
    return Result::kInvalidOperation;
  }

  rtc::scoped_refptr<webrtc::AudioSourceInterface> audio_source =
      pc_factory->CreateAudioSource(cricket::AudioOptions());
  if (!audio_source) {
    RTC_LOG(LS_ERROR) << "Failed to create local audio source.";
    return Result::kUnknownError;
  }
  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track =
      pc_factory->CreateAudioTrack(track_name, audio_source);
  if (!audio_track) {
    RTC_LOG(LS_ERROR) << "Failed to create local audio track.";
    return Result::kUnknownError;
  }
  auto result = peer->AddLocalAudioTrack(std::move(audio_track),
                                         config->transceiver_interop_handle,
                                         config->track_interop_handle);
  if (result.ok()) {
    RefPtr<LocalAudioTrack>& audio_track_wrapper = result.value();
    RefPtr<AudioTransceiver> transceiver =
        audio_track_wrapper->GetTransceiver();
    *track_handle_out = audio_track_wrapper.release();
    *transceiver_handle_out = transceiver.release();
    return Result::kSuccess;
  }
  RTC_LOG(LS_ERROR) << "Failed to add local video track to peer connection.";
  return Result::kUnknownError;
}

mrsResult MRS_CALL mrsPeerConnectionAddDataChannel(
    PeerConnectionHandle peerHandle,
    mrsDataChannelInteropHandle dataChannelInteropHandle,
    mrsDataChannelConfig config,
    mrsDataChannelCallbacks callbacks,
    DataChannelHandle* dataChannelHandleOut) noexcept

{
  if (!dataChannelHandleOut || !dataChannelInteropHandle) {
    return Result::kInvalidParameter;
  }
  *dataChannelHandleOut = nullptr;

  auto peer = static_cast<PeerConnection*>(peerHandle);
  if (!peer) {
    return Result::kInvalidNativeHandle;
  }

  const bool ordered = (config.flags & mrsDataChannelConfigFlags::kOrdered);
  const bool reliable = (config.flags & mrsDataChannelConfigFlags::kReliable);
  const std::string_view label = (config.label ? config.label : "");
  ErrorOr<std::shared_ptr<DataChannel>> data_channel = peer->AddDataChannel(
      config.id, label, ordered, reliable, dataChannelInteropHandle);
  if (data_channel.ok()) {
    data_channel.value()->SetMessageCallback(DataChannel::MessageCallback{
        callbacks.message_callback, callbacks.message_user_data});
    data_channel.value()->SetBufferingCallback(DataChannel::BufferingCallback{
        callbacks.buffering_callback, callbacks.buffering_user_data});
    data_channel.value()->SetStateCallback(DataChannel::StateCallback{
        callbacks.state_callback, callbacks.state_user_data});
    *dataChannelHandleOut = data_channel.value().operator->();
    return Result::kSuccess;
  }
  return data_channel.error().result();
}

mrsResult MRS_CALL mrsPeerConnectionRemoveLocalVideoTrack(
    PeerConnectionHandle peer_handle,
    LocalVideoTrackHandle track_handle) noexcept {
  auto peer = static_cast<PeerConnection*>(peer_handle);
  if (!peer) {
    return Result::kInvalidNativeHandle;
  }
  auto track = static_cast<LocalVideoTrack*>(track_handle);
  if (!track) {
    return Result::kInvalidNativeHandle;
  }
  return peer->RemoveLocalVideoTrack(*track);
}

mrsResult MRS_CALL mrsPeerConnectionRemoveLocalAudioTrack(
    PeerConnectionHandle peer_handle,
    LocalAudioTrackHandle track_handle) noexcept {
  auto peer = static_cast<PeerConnection*>(peer_handle);
  if (!peer) {
    return Result::kInvalidNativeHandle;
  }
  auto track = static_cast<LocalAudioTrack*>(track_handle);
  if (!track) {
    return Result::kInvalidNativeHandle;
  }
  return peer->RemoveLocalAudioTrack(*track);
}

mrsResult MRS_CALL mrsPeerConnectionRemoveDataChannel(
    PeerConnectionHandle peerHandle,
    DataChannelHandle dataChannelHandle) noexcept {
  auto peer = static_cast<PeerConnection*>(peerHandle);
  if (!peer) {
    return Result::kInvalidNativeHandle;
  }
  auto data_channel = static_cast<DataChannel*>(dataChannelHandle);
  if (!data_channel) {
    return Result::kInvalidNativeHandle;
  }
  peer->RemoveDataChannel(*data_channel);
  return Result::kSuccess;
}

mrsResult MRS_CALL
mrsDataChannelSendMessage(DataChannelHandle dataChannelHandle,
                          const void* data,
                          uint64_t size) noexcept {
  auto data_channel = static_cast<DataChannel*>(dataChannelHandle);
  if (!data_channel) {
    return Result::kInvalidNativeHandle;
  }
  return (data_channel->Send(data, (size_t)size) ? Result::kSuccess
                                                 : Result::kUnknownError);
}

mrsResult MRS_CALL
mrsPeerConnectionAddIceCandidate(PeerConnectionHandle peerHandle,
                                 const char* sdp,
                                 const int sdp_mline_index,
                                 const char* sdp_mid) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    return (peer->AddIceCandidate(sdp, sdp_mline_index, sdp_mid)
                ? Result::kSuccess
                : Result::kUnknownError);
  }
  return Result::kInvalidNativeHandle;
}

mrsResult MRS_CALL
mrsPeerConnectionCreateOffer(PeerConnectionHandle peerHandle) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    return (peer->CreateOffer() ? Result::kSuccess : Result::kUnknownError);
  }
  return Result::kInvalidNativeHandle;
}

mrsResult MRS_CALL
mrsPeerConnectionCreateAnswer(PeerConnectionHandle peerHandle) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    return (peer->CreateAnswer() ? Result::kSuccess : Result::kUnknownError);
  }
  return Result::kInvalidNativeHandle;
}

mrsResult MRS_CALL mrsPeerConnectionSetBitrate(PeerConnectionHandle peer_handle,
                                               int min_bitrate_bps,
                                               int start_bitrate_bps,
                                               int max_bitrate_bps) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peer_handle)) {
    BitrateSettings settings{};
    if (min_bitrate_bps >= 0) {
      settings.min_bitrate_bps = min_bitrate_bps;
    }
    if (start_bitrate_bps >= 0) {
      settings.start_bitrate_bps = start_bitrate_bps;
    }
    if (max_bitrate_bps >= 0) {
      settings.max_bitrate_bps = max_bitrate_bps;
    }
    return peer->SetBitrate(settings);
  }
  return Result::kInvalidNativeHandle;
}

mrsResult MRS_CALL
mrsPeerConnectionSetRemoteDescription(PeerConnectionHandle peerHandle,
                                      const char* type,
                                      const char* sdp) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    return (peer->SetRemoteDescription(type, sdp) ? Result::kSuccess
                                                  : Result::kUnknownError);
  }
  return Result::kInvalidNativeHandle;
}

mrsResult MRS_CALL
mrsPeerConnectionClose(PeerConnectionHandle peerHandle) noexcept {
  if (auto peer = static_cast<PeerConnection*>(peerHandle)) {
    peer->Close();
    return Result::kSuccess;
  }
  return Result::kInvalidNativeHandle;
}

mrsResult MRS_CALL mrsSdpForceCodecs(const char* message,
                                     SdpFilter audio_filter,
                                     SdpFilter video_filter,
                                     char* buffer,
                                     uint64_t* buffer_size) noexcept {
  RTC_CHECK(message);
  RTC_CHECK(buffer);
  RTC_CHECK(buffer_size);
  std::string message_str(message);
  std::string audio_codec_name_str;
  std::string video_codec_name_str;
  std::map<std::string, std::string> extra_audio_params;
  std::map<std::string, std::string> extra_video_params;
  if (audio_filter.codec_name) {
    audio_codec_name_str.assign(audio_filter.codec_name);
  }
  if (video_filter.codec_name) {
    video_codec_name_str.assign(video_filter.codec_name);
  }
  // Only assign extra parameters if codec name is not empty
  if (!audio_codec_name_str.empty() && audio_filter.params) {
    SdpParseCodecParameters(audio_filter.params, extra_audio_params);
  }
  if (!video_codec_name_str.empty() && video_filter.params) {
    SdpParseCodecParameters(video_filter.params, extra_video_params);
  }
  std::string out_message =
      SdpForceCodecs(message_str, audio_codec_name_str, extra_audio_params,
                     video_codec_name_str, extra_video_params);
  const size_t capacity = static_cast<size_t>(*buffer_size);
  const size_t size = out_message.size();
  *buffer_size = size + 1;
  if (capacity < size + 1) {
    return Result::kInvalidParameter;
  }
  memcpy(buffer, out_message.c_str(), size);
  buffer[size] = '\0';
  return Result::kSuccess;
}

void MRS_CALL mrsSetFrameHeightRoundMode(FrameHeightRoundMode value) {
  PeerConnection::SetFrameHeightRoundMode(
      (PeerConnection::FrameHeightRoundMode)value);
}

void MRS_CALL mrsMemCpy(void* dst, const void* src, uint64_t size) noexcept {
  memcpy(dst, src, static_cast<size_t>(size));
}

void MRS_CALL mrsMemCpyStride(void* dst,
                              int32_t dst_stride,
                              const void* src,
                              int32_t src_stride,
                              int32_t elem_size,
                              int32_t elem_count) noexcept {
  RTC_CHECK(dst);
  RTC_CHECK(dst_stride >= elem_size);
  RTC_CHECK(src);
  RTC_CHECK(src_stride >= elem_size);
  if ((dst_stride == elem_size) && (src_stride == elem_size)) {
    // If tightly packed, do a single memcpy() for performance
    const size_t total_size = (size_t)elem_size * elem_count;
    memcpy(dst, src, total_size);
  } else {
    // Otherwise, copy row by row
    for (int i = 0; i < elem_count; ++i) {
      memcpy(dst, src, elem_size);
      dst = (char*)dst + dst_stride;
      src = (const char*)src + src_stride;
    }
  }
}

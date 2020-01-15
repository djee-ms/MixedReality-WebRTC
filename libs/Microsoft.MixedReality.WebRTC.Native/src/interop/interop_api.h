// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#if defined(WINUWP)
// Non-API helper. Returned object can be deleted at any time in theory.
// In practice because it's provided by a global object it's safe.
//< TODO - Remove that, clean-up API, this is bad (c).
rtc::Thread* UnsafeGetWorkerThread();
#endif

#include "audio_frame.h"
#include "export.h"
#include "mrs_errors.h"
#include "video_frame.h"

extern "C" {

/// 32-bit boolean for interop API.
enum class mrsBool : int32_t { kTrue = -1, kFalse = 0 };

using mrsResult = Microsoft::MixedReality::WebRTC::Result;

//
// Generic utilities
//

/// Global MixedReality-WebRTC library shutdown options.
enum class mrsShutdownOptions : uint32_t {
  kNone = 0,

  /// Fail to shutdown if some objects are still alive. This is set by default
  /// and provides safety against deadlocking, since WebRTC calls are proxied on
  /// background threads which would be destroyed by the shutdown, causing
  /// objects still alive to deadlock when making such calls. Note however that
  /// keeping the library alive, and in particular the WebRTC background
  /// threads, means that the module (DLL) cannot be unloaded, which might be
  /// problematic in some use cases (e.g. Unity Editor hot-reload).
  kFailOnLiveObjects = 0x1,

  /// Log some report about live objects when trying to shutdown, to help
  /// debugging.
  kLogLiveObjects = 0x2
};

/// Set options for the automatic shutdown of the MixedReality-WebRTC library.
/// This enables controlling the behavior of the library when it is shut down as
/// a result of all tracked objects being released, or when the program
/// terminates.
MRS_API void MRS_CALL
mrsSetShutdownOptions(mrsShutdownOptions options) noexcept;

/// Opaque enumerator type.
struct mrsEnumerator;

/// Handle to an enumerator.
/// This must be freed after use with |mrsCloseEnum|.
using mrsEnumHandle = mrsEnumerator*;

/// Close an enumerator previously obtained from one of the EnumXxx() calls.
MRS_API void MRS_CALL mrsCloseEnum(mrsEnumHandle* handleRef) noexcept;

//
// Interop
//

struct mrsAudioTransceiverConfig;
struct mrsVideoTransceiverConfig;
struct mrsRemoteAudioTrackConfig;
struct mrsRemoteVideoTrackConfig;
struct mrsDataChannelConfig;
struct mrsDataChannelCallbacks;

/// Opaque handle to the interop wrapper of a peer connection.
using mrsPeerConnectionInteropHandle = void*;

/// Opaque handle to the interop wrapper of an audio transceiver.
using mrsAudioTransceiverInteropHandle = void*;

/// Opaque handle to the interop wrapper of a video transceiver.
using mrsVideoTransceiverInteropHandle = void*;

/// Opaque handle to the interop wrapper of a local audio track.
using mrsLocalAudioTrackInteropHandle = void*;

/// Opaque handle to the interop wrapper of a local video track.
using mrsLocalVideoTrackInteropHandle = void*;

/// Opaque handle to the interop wrapper of a remote audio track.
using mrsRemoteAudioTrackInteropHandle = void*;

/// Opaque handle to the interop wrapper of a remote video track.
using mrsRemoteVideoTrackInteropHandle = void*;

/// Opaque handle to the interop wrapper of a data channel.
using mrsDataChannelInteropHandle = void*;

/// Callback to create an interop wrapper for an audio tranceiver.
using mrsAudioTransceiverCreateObjectCallback =
    mrsAudioTransceiverInteropHandle(MRS_CALL*)(
        mrsPeerConnectionInteropHandle parent,
        const mrsAudioTransceiverConfig& config) noexcept;

/// Callback to create an interop wrapper for a video tranceiver.
using mrsVideoTransceiverCreateObjectCallback =
    mrsVideoTransceiverInteropHandle(MRS_CALL*)(
        mrsPeerConnectionInteropHandle parent,
        const mrsVideoTransceiverConfig& config) noexcept;

/// Callback to create an interop wrapper for a remote audio track.
using mrsRemoteAudioTrackCreateObjectCallback =
    mrsRemoteAudioTrackInteropHandle(MRS_CALL*)(
        mrsPeerConnectionInteropHandle parent,
        const mrsRemoteAudioTrackConfig& config) noexcept;

/// Callback to create an interop wrapper for a remote video track.
using mrsRemoteVideoTrackCreateObjectCallback =
    mrsRemoteVideoTrackInteropHandle(MRS_CALL*)(
        mrsPeerConnectionInteropHandle parent,
        const mrsRemoteVideoTrackConfig& config) noexcept;

/// Callback to create an interop wrapper for a data channel.
using mrsDataChannelCreateObjectCallback = mrsDataChannelInteropHandle(
    MRS_CALL*)(mrsPeerConnectionInteropHandle parent,
               const mrsDataChannelConfig& config,
               mrsDataChannelCallbacks* callbacks) noexcept;

//
// Video capture enumeration
//

/// Callback invoked for each enumerated video capture device.
using mrsVideoCaptureDeviceEnumCallback = void(MRS_CALL*)(const char* id,
                                                          const char* name,
                                                          void* user_data);

/// Callback invoked on video capture device enumeration completed.
using mrsVideoCaptureDeviceEnumCompletedCallback =
    void(MRS_CALL*)(void* user_data);

/// Enumerate the video capture devices asynchronously.
/// For each device found, invoke the mandatory |callback|.
/// At the end of the enumeration, invoke the optional |completedCallback| if it
/// was provided (non-null).
MRS_API mrsResult MRS_CALL mrsEnumVideoCaptureDevicesAsync(
    mrsVideoCaptureDeviceEnumCallback enumCallback,
    void* enumCallbackUserData,
    mrsVideoCaptureDeviceEnumCompletedCallback completedCallback,
    void* completedCallbackUserData) noexcept;

/// Callback invoked for each enumerated video capture format.
using mrsVideoCaptureFormatEnumCallback = void(MRS_CALL*)(uint32_t width,
                                                          uint32_t height,
                                                          double framerate,
                                                          uint32_t encoding,
                                                          void* user_data);

/// Callback invoked on video capture format enumeration completed.
using mrsVideoCaptureFormatEnumCompletedCallback =
    void(MRS_CALL*)(mrsResult result, void* user_data);

/// Enumerate the video capture formats asynchronously.
/// For each device found, invoke the mandatory |callback|.
/// At the end of the enumeration, invoke the optional |completedCallback| if it
/// was provided (non-null).
MRS_API mrsResult MRS_CALL mrsEnumVideoCaptureFormatsAsync(
    const char* device_id,
    mrsVideoCaptureFormatEnumCallback enumCallback,
    void* enumCallbackUserData,
    mrsVideoCaptureFormatEnumCompletedCallback completedCallback,
    void* completedCallbackUserData) noexcept;

//
// Peer connection
//

/// Opaque handle to a native PeerConnection C++ object.
using PeerConnectionHandle = void*;

/// Opaque handle to a native MediaTrack C++ object.
using MediaTrackHandle = void*;

/// Opaque handle to a native AudioTransceiver C++ object.
using AudioTransceiverHandle = void*;

/// Opaque handle to a native VideoTransceiver C++ object.
using VideoTransceiverHandle = void*;

/// Opaque handle to a native LocalAudioTrack C++ object.
using LocalAudioTrackHandle = void*;

/// Opaque handle to a native LocalVideoTrack C++ object.
using LocalVideoTrackHandle = void*;

/// Opaque handle to a native RemoteAudioTrack C++ object.
using RemoteAudioTrackHandle = void*;

/// Opaque handle to a native RemoteVideoTrack C++ object.
using RemoteVideoTrackHandle = void*;

/// Opaque handle to a native DataChannel C++ object.
using DataChannelHandle = void*;

/// Opaque handle to a native ExternalVideoTrackSource C++ object.
using ExternalVideoTrackSourceHandle = void*;

/// Callback fired when the peer connection is connected, that is it finished
/// the JSEP offer/answer exchange successfully.
using PeerConnectionConnectedCallback = void(MRS_CALL*)(void* user_data);

/// Callback fired when a local SDP message has been prepared and is ready to be
/// sent by the user via the signaling service.
using PeerConnectionLocalSdpReadytoSendCallback =
    void(MRS_CALL*)(void* user_data, const char* type, const char* sdp_data);

/// Callback fired when an ICE candidate has been prepared and is ready to be
/// sent by the user via the signaling service.
using PeerConnectionIceCandidateReadytoSendCallback =
    void(MRS_CALL*)(void* user_data,
                    const char* candidate,
                    int sdpMlineindex,
                    const char* sdpMid);

/// State of the ICE connection.
/// See https://www.w3.org/TR/webrtc/#rtciceconnectionstate-enum.
/// Note that there is a mismatch currently due to the m71 implementation.
enum class IceConnectionState : int32_t {
  kNew = 0,
  kChecking = 1,
  kConnected = 2,
  kCompleted = 3,
  kFailed = 4,
  kDisconnected = 5,
  kClosed = 6,
};

/// State of the ICE gathering process.
/// See https://www.w3.org/TR/webrtc/#rtcicegatheringstate-enum
enum class IceGatheringState : int32_t {
  kNew = 0,
  kGathering = 1,
  kComplete = 2,
};

/// Callback fired when the state of the ICE connection changed.
using PeerConnectionIceStateChangedCallback =
    void(MRS_CALL*)(void* user_data, IceConnectionState new_state);

/// Callback fired when a renegotiation of the current session needs to occur to
/// account for new parameters (e.g. added or removed tracks).
using PeerConnectionRenegotiationNeededCallback =
    void(MRS_CALL*)(void* user_data);

/// Kind of media track. Equivalent to
/// webrtc::MediaStreamTrackInterface::kind().
enum class TrackKind : uint32_t {
  kUnknownTrack = 0,
  kAudioTrack = 1,
  kVideoTrack = 2,
  kDataTrack = 3,
};

/// Callback fired when a remote audio track is added to a connection.
/// The |audio_track| and |audio_transceiver| handle hold a reference to the
/// underlying native object they are associated with, and therefore must be
/// released with |mrsLocalAudioTrackRemoveRef()| and
/// |mrsAudioTransceiverRemoveRef()|, respectively, to avoid memory leaks.
using PeerConnectionAudioTrackAddedCallback =
    void(MRS_CALL*)(mrsPeerConnectionInteropHandle peer,
                    mrsRemoteAudioTrackInteropHandle audio_track_wrapper,
                    RemoteAudioTrackHandle audio_track,
                    mrsAudioTransceiverInteropHandle audio_transceiver_wrapper,
                    AudioTransceiverHandle audio_transceiver);

/// Callback fired when a remote audio track is removed from a connection.
/// The |audio_track| and |audio_transceiver| handle hold a reference to the
/// underlying native object they are associated with, and therefore must be
/// released with |mrsLocalAudioTrackRemoveRef()| and
/// |mrsAudioTransceiverRemoveRef()|, respectively, to avoid memory leaks.
using PeerConnectionAudioTrackRemovedCallback =
    void(MRS_CALL*)(mrsPeerConnectionInteropHandle peer,
                    mrsRemoteAudioTrackInteropHandle audio_track_wrapper,
                    RemoteAudioTrackHandle audio_track,
                    mrsAudioTransceiverInteropHandle audio_transceiver_wrapper,
                    AudioTransceiverHandle audio_transceiver);

/// Callback fired when a remote video track is added to a connection.
/// The |video_track| and |video_transceiver| handle hold a reference to the
/// underlying native object they are associated with, and therefore must be
/// released with |mrsLocalVideoTrackRemoveRef()| and
/// |mrsVideoTransceiverRemoveRef()|, respectively, to avoid memory leaks.
using PeerConnectionVideoTrackAddedCallback =
    void(MRS_CALL*)(mrsPeerConnectionInteropHandle peer,
                    mrsRemoteVideoTrackInteropHandle video_track_wrapper,
                    RemoteVideoTrackHandle video_track,
                    mrsVideoTransceiverInteropHandle video_transceiver_wrapper,
                    VideoTransceiverHandle video_transceiver);

/// Callback fired when a remote video track is removed from a connection.
/// The |video_track| and |video_transceiver| handle hold a reference to the
/// underlying native object they are associated with, and therefore must be
/// released with |mrsLocalVideoTrackRemoveRef()| and
/// |mrsVideoTransceiverRemoveRef()|, respectively, to avoid memory leaks.
using PeerConnectionVideoTrackRemovedCallback =
    void(MRS_CALL*)(mrsPeerConnectionInteropHandle peer,
                    mrsRemoteVideoTrackInteropHandle video_track_wrapper,
                    RemoteVideoTrackHandle video_track,
                    mrsVideoTransceiverInteropHandle video_transceiver_wrapper,
                    VideoTransceiverHandle video_transceiver);

/// Callback fired when a data channel is added to the peer connection after
/// being negotiated with the remote peer.
using PeerConnectionDataChannelAddedCallback =
    void(MRS_CALL*)(mrsPeerConnectionInteropHandle peer,
                    mrsDataChannelInteropHandle data_channel_wrapper,
                    DataChannelHandle data_channel);

/// Callback fired when a data channel is remoted from the peer connection.
using PeerConnectionDataChannelRemovedCallback =
    void(MRS_CALL*)(mrsPeerConnectionInteropHandle peer,
                    mrsDataChannelInteropHandle data_channel_wrapper,
                    DataChannelHandle data_channel);

using mrsI420AVideoFrame = Microsoft::MixedReality::WebRTC::I420AVideoFrame;

/// Callback fired when a local or remote (depending on use) video frame is
/// available to be consumed by the caller, usually for display.
/// The video frame is encoded in I420 triplanar format (NV12).
using mrsI420AVideoFrameCallback =
    void(MRS_CALL*)(void* user_data, const mrsI420AVideoFrame& frame);

using mrsArgb32VideoFrame = Microsoft::MixedReality::WebRTC::Argb32VideoFrame;

/// Callback fired when a local or remote (depending on use) video frame is
/// available to be consumed by the caller, usually for display.
/// The video frame is encoded in ARGB 32-bit per pixel.
using mrsArgb32VideoFrameCallback =
    void(MRS_CALL*)(void* user_data, const mrsArgb32VideoFrame& frame);

using mrsAudioFrame = Microsoft::MixedReality::WebRTC::AudioFrame;

/// Callback fired when a local or remote (depending on use) audio frame is
/// available to be consumed by the caller, usually for local output.
using mrsAudioFrameCallback = void(MRS_CALL*)(void* user_data,
                                              const mrsAudioFrame& frame);

/// Callback fired when a message is received on a data channel.
using mrsDataChannelMessageCallback = void(MRS_CALL*)(void* user_data,
                                                      const void* data,
                                                      const uint64_t size);

/// Callback fired when a data channel buffering changes.
/// The |previous| and |current| values are the old and new sizes in byte of the
/// buffering buffer. The |limit| is the capacity of the buffer.
/// Note that when the buffer is full, any attempt to send data will result is
/// an abrupt closing of the data channel. So monitoring this state is critical.
using mrsDataChannelBufferingCallback = void(MRS_CALL*)(void* user_data,
                                                        const uint64_t previous,
                                                        const uint64_t current,
                                                        const uint64_t limit);

/// Callback fired when the state of a data channel changed.
using mrsDataChannelStateCallback = void(MRS_CALL*)(void* user_data,
                                                    int32_t state,
                                                    int32_t id);

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

/// Configuration to intialize a peer connection object.
struct PeerConnectionConfiguration {
  /// ICE servers, encoded as a single string buffer.
  /// See |EncodeIceServers| and |DecodeIceServers|.
  const char* encoded_ice_servers = nullptr;

  /// ICE transport type for the connection.
  IceTransportType ice_transport_type = IceTransportType::kAll;

  /// Bundle policy for the connection.
  BundlePolicy bundle_policy = BundlePolicy::kBalanced;

  /// SDP semantic for connection negotiation.
  /// Do not use Plan B unless there is a problem with Unified Plan.
  SdpSemantic sdp_semantic = SdpSemantic::kUnifiedPlan;
};

/// Create a peer connection and return a handle to it.
/// On UWP this must be invoked from another thread than the main UI thread.
/// The newly-created peer connection native resource is reference-counted, and
/// has a single reference when this function returns. Additional references may
/// be added with |mrsPeerConnectionAddRef| and removed with
/// |mrsPeerConnectionRemoveRef|. When the last reference is removed, the native
/// object is destroyed.
MRS_API mrsResult MRS_CALL
mrsPeerConnectionCreate(PeerConnectionConfiguration config,
                        mrsPeerConnectionInteropHandle interop_handle,
                        PeerConnectionHandle* peerHandleOut) noexcept;

/// Callbacks needed to allow the native implementation to interact with the
/// interop layer, and in particular to react to events which necessitate
/// creating a new interop wrapper for a new native instance (whose creation was
/// not initiated by the interop, so for which the native instance is created
/// first).
struct mrsPeerConnectionInteropCallbacks {
  /// Construct an interop object for an AudioTransceiver instance.
  mrsAudioTransceiverCreateObjectCallback audio_transceiver_create_object{};

  /// Construct an interop object for a VideoTransceiver instance.
  mrsVideoTransceiverCreateObjectCallback video_transceiver_create_object{};

  /// Construct an interop object for a RemoteAudioTrack instance.
  mrsRemoteAudioTrackCreateObjectCallback remote_audio_track_create_object{};

  /// Construct an interop object for a RemoteVideooTrack instance.
  mrsRemoteVideoTrackCreateObjectCallback remote_video_track_create_object{};

  /// Construct an interop object for a DataChannel instance.
  mrsDataChannelCreateObjectCallback data_channel_create_object{};
};

/// Register the interop callbacks necessary to make interop work. To
/// unregister, simply pass nullptr as the callback pointer. Only one set of
/// callbacks can be registered at a time.
MRS_API mrsResult MRS_CALL mrsPeerConnectionRegisterInteropCallbacks(
    PeerConnectionHandle peerHandle,
    mrsPeerConnectionInteropCallbacks* callbacks) noexcept;

/// Register a callback invoked once connected to a remote peer. To unregister,
/// simply pass nullptr as the callback pointer. Only one callback can be
/// registered at a time.
MRS_API void MRS_CALL mrsPeerConnectionRegisterConnectedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionConnectedCallback callback,
    void* user_data) noexcept;

/// Register a callback invoked when a local message is ready to be sent via the
/// signaling service to a remote peer. Only one callback can be registered at a
/// time.
MRS_API void MRS_CALL mrsPeerConnectionRegisterLocalSdpReadytoSendCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionLocalSdpReadytoSendCallback callback,
    void* user_data) noexcept;

/// Register a callback invoked when an ICE candidate message is ready to be
/// sent via the signaling service to a remote peer. Only one callback can be
/// registered at a time.
MRS_API void MRS_CALL mrsPeerConnectionRegisterIceCandidateReadytoSendCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionIceCandidateReadytoSendCallback callback,
    void* user_data) noexcept;

/// Register a callback invoked when the ICE connection state changes. Only one
/// callback can be registered at a time.
MRS_API void MRS_CALL mrsPeerConnectionRegisterIceStateChangedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionIceStateChangedCallback callback,
    void* user_data) noexcept;

/// Register a callback fired when a renegotiation of the current session needs
/// to occur to account for new parameters (e.g. added or removed tracks).
MRS_API void MRS_CALL mrsPeerConnectionRegisterRenegotiationNeededCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionRenegotiationNeededCallback callback,
    void* user_data) noexcept;

/// Register a callback fired when a remote audio track is added to the current
/// peer connection.
/// Note that the arguments include some object handles, which each hold a
/// reference to the corresponding object and therefore must be released, even
/// if the user does not make use of them in the callback.
MRS_API void MRS_CALL mrsPeerConnectionRegisterAudioTrackAddedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionAudioTrackAddedCallback callback,
    void* user_data) noexcept;

/// Register a callback fired when a remote audio track is removed from the
/// current peer connection.
/// Note that the arguments include some object handles, which each hold a
/// reference to the corresponding object and therefore must be released, even
/// if the user does not make use of them in the callback.
MRS_API void MRS_CALL mrsPeerConnectionRegisterAudioTrackRemovedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionAudioTrackRemovedCallback callback,
    void* user_data) noexcept;

/// Register a callback fired when a remote video track is added to the current
/// peer connection.
/// Note that the arguments include some object handles, which each hold a
/// reference to the corresponding object and therefore must be released, even
/// if the user does not make use of them in the callback.
MRS_API void MRS_CALL mrsPeerConnectionRegisterVideoTrackAddedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionVideoTrackAddedCallback callback,
    void* user_data) noexcept;

/// Register a callback fired when a remote video track is removed from the
/// current peer connection.
/// Note that the arguments include some object handles, which each hold a
/// reference to the corresponding object and therefore must be released, even
/// if the user does not make use of them in the callback.
MRS_API void MRS_CALL mrsPeerConnectionRegisterVideoTrackRemovedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionVideoTrackRemovedCallback callback,
    void* user_data) noexcept;

/// Register a callback fired when a remote data channel is removed from the
/// current peer connection.
MRS_API void MRS_CALL mrsPeerConnectionRegisterDataChannelAddedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionDataChannelAddedCallback callback,
    void* user_data) noexcept;

/// Register a callback fired when a remote data channel is removed from the
/// current peer connection.
MRS_API void MRS_CALL mrsPeerConnectionRegisterDataChannelRemovedCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionDataChannelRemovedCallback callback,
    void* user_data) noexcept;

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

/// Configuration for creating a new audio transceiver.
struct AudioTransceiverInitConfig {
  /// Name of the audio transceiver.
  const char* name = nullptr;

  /// Handle of the audio transceiver interop wrapper, if any, which will be
  /// associated with the native audio transceiver object.
  mrsAudioTransceiverInteropHandle transceiver_interop_handle{};
};

/// Configuration for creating a new video transceiver.
struct VideoTransceiverInitConfig {
  /// Name of the video transceiver.
  const char* name = nullptr;

  /// Handle of the video transceiver interop wrapper, if any, which will be
  /// associated with the native video transceiver object.
  mrsVideoTransceiverInteropHandle transceiver_interop_handle{};
};

/// Configuration for opening a local audio capture device and creating a local
/// audio track.
struct LocalAudioTrackInitConfig {
  /// Handle of the local audio track interop wrapper, if any, which will be
  /// associated with the native local audio track object.
  mrsLocalAudioTrackInteropHandle track_interop_handle{};
};

/// Configuration for opening a local video capture device and creating a local
/// video track.
struct LocalVideoTrackInitConfig {
  /// Handle of the local video track interop wrapper, if any, which will be
  /// associated with the native local video track object.
  mrsLocalVideoTrackInteropHandle track_interop_handle{};

  /// Unique identifier of the video capture device to select, as returned by
  /// |mrsEnumVideoCaptureDevicesAsync|, or a null or empty string to select the
  /// default device.
  const char* video_device_id = nullptr;

  /// Optional name of a video profile, if the platform supports it, or null to
  /// no use video profiles.
  const char* video_profile_id = nullptr;

  /// Optional kind of video profile to select, if the platform supports it.
  /// If a video profile ID is specified with |video_profile_id| it is
  /// recommended to leave this as kUnspecified to avoid over-constraining the
  /// video capture format selection.
  VideoProfileKind video_profile_kind = VideoProfileKind::kUnspecified;

  /// Optional preferred capture resolution width, in pixels, or zero for
  /// unconstrained.
  uint32_t width = 0;

  /// Optional preferred capture resolution height, in pixels, or zero for
  /// unconstrained.
  uint32_t height = 0;

  /// Optional preferred capture framerate, in frame per second (FPS), or zero
  /// for unconstrained.
  /// This framerate is compared exactly to the one reported by the video
  /// capture device (webcam), so should be queried rather than hard-coded to
  /// avoid mismatches with video formats reporting e.g. 29.99 instead of 30.0.
  double framerate = 0;

  /// On platforms supporting Mixed Reality Capture (MRC) like HoloLens, enable
  /// this feature. This produces a video track where the holograms rendering is
  /// overlaid over the webcam frame. This parameter is ignored on platforms not
  /// supporting MRC.
  /// Note that MRC is only available in exclusive-mode applications, or in
  /// shared apps with the restricted capability "rescap:screenDuplication". In
  /// any other case the capability will not be granted and MRC will silently
  /// fail, falling back to a simple webcam video feed without holograms.
  mrsBool enable_mrc = mrsBool::kTrue;

  /// When Mixed Reality Capture is enabled, enable or disable the recording
  /// indicator shown on screen.
  mrsBool enable_mrc_recording_indicator = mrsBool::kTrue;
};

/// Configuration for creating a local video track from an external source.
struct LocalVideoTrackFromExternalSourceInitConfig {
  /// Handle of the local video track interop wrapper, if any, which will be
  /// associated with the native local video track object.
  mrsLocalVideoTrackInteropHandle track_interop_handle{};

  /// Handle to the native source.
  ExternalVideoTrackSourceHandle source_handle{};
};

using mrsRequestExternalI420AVideoFrameCallback =
    mrsResult(MRS_CALL*)(void* user_data,
                         ExternalVideoTrackSourceHandle source_handle,
                         uint32_t request_id,
                         int64_t timestamp_ms);

using mrsRequestExternalArgb32VideoFrameCallback =
    mrsResult(MRS_CALL*)(void* user_data,
                         ExternalVideoTrackSourceHandle source_handle,
                         uint32_t request_id,
                         int64_t timestamp_ms);

struct mrsAudioTransceiverConfig {};

struct mrsVideoTransceiverConfig {};

struct mrsRemoteAudioTrackConfig {
  const char* track_name{};
};

struct mrsRemoteVideoTrackConfig {
  const char* track_name{};
};

enum class mrsDataChannelConfigFlags : uint32_t {
  kOrdered = 0x1,
  kReliable = 0x2,
};

inline mrsDataChannelConfigFlags operator|(
    mrsDataChannelConfigFlags a,
    mrsDataChannelConfigFlags b) noexcept {
  return (mrsDataChannelConfigFlags)((uint32_t)a | (uint32_t)b);
}

inline uint32_t operator&(mrsDataChannelConfigFlags a,
                          mrsDataChannelConfigFlags b) noexcept {
  return ((uint32_t)a | (uint32_t)b);
}

struct mrsDataChannelConfig {
  int32_t id = -1;      // -1 for auto; >=0 for negotiated
  const char* label{};  // optional; can be null or empty string
  mrsDataChannelConfigFlags flags{};
};

struct mrsDataChannelCallbacks {
  mrsDataChannelMessageCallback message_callback{};
  void* message_user_data{};
  mrsDataChannelBufferingCallback buffering_callback{};
  void* buffering_user_data{};
  mrsDataChannelStateCallback state_callback{};
  void* state_user_data{};
};

/// Add a new data channel.
/// This function has two distinct uses:
/// - If id < 0, then it adds a new in-band data channel with an ID that will be
/// selected by the WebRTC implementation itself, and will be available later.
/// In that case the channel is announced to the remote peer for it to create a
/// channel with the same ID.
/// - If id >= 0, then it adds a new out-of-band negotiated channel with the
/// given ID, and it is the responsibility of the app to create a channel with
/// the same ID on the remote peer to be able to use the channel.
MRS_API mrsResult MRS_CALL mrsPeerConnectionAddDataChannel(
    PeerConnectionHandle peerHandle,
    mrsDataChannelInteropHandle dataChannelInteropHandle,
    mrsDataChannelConfig config,
    mrsDataChannelCallbacks callbacks,
    DataChannelHandle* dataChannelHandleOut) noexcept;

MRS_API mrsResult MRS_CALL mrsPeerConnectionRemoveDataChannel(
    PeerConnectionHandle peerHandle,
    DataChannelHandle dataChannelHandle) noexcept;

MRS_API mrsResult MRS_CALL
mrsDataChannelSendMessage(DataChannelHandle dataChannelHandle,
                          const void* data,
                          uint64_t size) noexcept;

/// Add a new ICE candidate received from a signaling service.
MRS_API mrsResult MRS_CALL
mrsPeerConnectionAddIceCandidate(PeerConnectionHandle peerHandle,
                                 const char* sdp_mid,
                                 const int sdp_mline_index,
                                 const char* candidate) noexcept;

/// Create a new JSEP offer to try to establish a connection with a remote peer.
/// This will generate a local offer message, then fire the
/// "LocalSdpReadytoSendCallback" callback, which should send this message via
/// the signaling service to a remote peer.
MRS_API mrsResult MRS_CALL
mrsPeerConnectionCreateOffer(PeerConnectionHandle peerHandle) noexcept;

/// Create a new JSEP answer to a received offer to try to establish a
/// connection with a remote peer. This will generate a local answer message,
/// then fire the "LocalSdpReadytoSendCallback" callback, which should send this
/// message via the signaling service to a remote peer.
MRS_API mrsResult MRS_CALL
mrsPeerConnectionCreateAnswer(PeerConnectionHandle peerHandle) noexcept;

/// Set the bitrate allocated to all RTP streams sent by this connection.
/// Other limitations might affect these limits and are respected (for example
/// "b=AS" in SDP).
///
/// Setting |start_bitrate_bps| will reset the current bitrate estimate to the
/// provided value.
///
/// The values are in bits per second.
/// If any of the arguments has a negative value, it will be ignored.
MRS_API mrsResult MRS_CALL
mrsPeerConnectionSetBitrate(PeerConnectionHandle peer_handle,
                            int min_bitrate_bps,
                            int start_bitrate_bps,
                            int max_bitrate_bps) noexcept;

/// Set a remote description received from a remote peer via the signaling
/// service.
MRS_API mrsResult MRS_CALL
mrsPeerConnectionSetRemoteDescription(PeerConnectionHandle peerHandle,
                                      const char* type,
                                      const char* sdp) noexcept;

/// Close a peer connection, removing all tracks and disconnecting from the
/// remote peer currently connected. This does not invalidate the handle nor
/// destroy the native peer connection object, but leaves it in a state where it
/// can only be destroyed.
MRS_API mrsResult MRS_CALL
mrsPeerConnectionClose(PeerConnectionHandle peerHandle) noexcept;

//
// SDP utilities
//

/// Codec arguments for SDP filtering, to allow selecting a preferred codec and
/// overriding some of its parameters.
struct SdpFilter {
  /// SDP name of a preferred codec, which is to be retained alone if present in
  /// the SDP offer message, discarding all others.
  const char* codec_name = nullptr;

  /// Semicolon-separated list of "key=value" pairs of codec parameters to pass
  /// to the codec. Arguments are passed as is without validation of their name
  /// nor value.
  const char* params = nullptr;
};

/// Force audio and video codecs when advertizing capabilities in an SDP offer.#
///
/// This is a workaround for the lack of access to codec selection. Instead of
/// selecting codecs in code, this can be used to intercept a generated SDP
/// offer before it is sent to the remote peer, and modify it by removing the
/// codecs the user does not want.
///
/// Codec names are compared to the list of supported codecs in the input
/// message string, and if found then other codecs are pruned out. If the codec
/// name is not found, the codec is assumed to be unsupported, so codecs for
/// that type are not modified.
///
/// On return the SDP offer message string to be sent via the signaler is stored
/// into the output buffer pointed to by |buffer|.
///
/// Note that because this function always return a message shorter or equal to
/// the input message, one way to ensure this function doesn't fail is to pass
/// an output buffer as large as the input message.
///
/// |message| SDP message string to deserialize.
/// |audio_codec_name| Optional SDP name of the audio codec to
/// force if supported, or nullptr or empty string to leave unmodified.
/// |video_codec_name| Optional SDP name of the video codec to force if
/// supported, or nullptr or empty string to leave unmodified.
/// |buffer| Output buffer of capacity *|buffer_size|.
/// |buffer_size| Pointer to the buffer capacity on input, modified on output
/// with the actual size of the null-terminated string, including the null
/// terminator, so the size of the used part of the buffer, in bytes.
/// Returns true on success or false if the buffer is not large enough to
/// contain the new SDP message.
MRS_API mrsResult MRS_CALL mrsSdpForceCodecs(const char* message,
                                             SdpFilter audio_filter,
                                             SdpFilter video_filter,
                                             char* buffer,
                                             uint64_t* buffer_size) noexcept;

/// Must be the same as PeerConnection::FrameHeightRoundMode.
enum class FrameHeightRoundMode : int32_t { kNone = 0, kCrop = 1, kPad = 2 };

/// See PeerConnection::SetFrameHeightRoundMode.
MRS_API void MRS_CALL mrsSetFrameHeightRoundMode(FrameHeightRoundMode value);

//
// Generic utilities
//

/// Optimized helper to copy a contiguous block of memory.
/// This is equivalent to the standard malloc() function.
MRS_API void MRS_CALL mrsMemCpy(void* dst,
                                const void* src,
                                uint64_t size) noexcept;

/// Optimized helper to copy a block of memory with source and destination
/// stride.
MRS_API void MRS_CALL mrsMemCpyStride(void* dst,
                                      int32_t dst_stride,
                                      const void* src,
                                      int32_t src_stride,
                                      int32_t elem_size,
                                      int32_t elem_count) noexcept;

}  // extern "C"

inline mrsShutdownOptions operator|(mrsShutdownOptions a,
                                    mrsShutdownOptions b) noexcept {
  return (mrsShutdownOptions)((uint32_t)a | (uint32_t)b);
}

inline mrsShutdownOptions operator&(mrsShutdownOptions a,
                                    mrsShutdownOptions b) noexcept {
  return (mrsShutdownOptions)((uint32_t)a & (uint32_t)b);
}

inline bool operator==(mrsShutdownOptions a, uint32_t b) noexcept {
  return ((uint32_t)a == b);
}

inline bool operator!=(mrsShutdownOptions a, uint32_t b) noexcept {
  return ((uint32_t)a != b);
}

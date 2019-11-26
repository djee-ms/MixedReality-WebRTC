// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

//
// This file contains private utilities for interoperability between the
// MixedReality-WebRTC API and the webrtc core implementation.
//

#include "api/rtcerror.h"

#include "mrs_errors.h"

struct mrsVideoDeviceConfiguration;

namespace Microsoft::MixedReality::WebRTC {
struct VideoDeviceConfiguration;
}

namespace Microsoft::MixedReality::WebRTC::detail {

/// Check if a raw C string is NULL or empty.
inline bool IsStringNullOrEmpty(const char* str) noexcept {
  return ((str == nullptr) || (str[0] == '\0'));
}

/// Convert an RTCErrorType to a Result.
inline Result ResultFromRTCErrorType(webrtc::RTCErrorType type) {
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

/// Convert an RTCError to an Error.
inline Error ErrorFromRTCError(const webrtc::RTCError& error) {
  return Error(ResultFromRTCErrorType(error.type()), str(error.message()));
}

/// Convert an RTCError to an Error.
inline Error ErrorFromRTCError(webrtc::RTCError&& error) {
  // Ideally would move the std::string out of |error|, but doesn't look
  // possible at the moment.
  return Error(ResultFromRTCErrorType(error.type()), str(error.message()));
}

/// Generic conversion utility. Implemented only for some types; see the
/// explicit instantiations below.
template <typename U, typename T>
U FromInterop(const T&);

/// Create a VideoDeviceConfiguration from its interop counterpart.
/// The string views are shared between the two objects.
template <>
MRS_API VideoDeviceConfiguration
FromInterop<VideoDeviceConfiguration, mrsVideoDeviceConfiguration>(
    const mrsVideoDeviceConfiguration& interop);

/// Create an AudioDeviceConfiguration from its interop counterpart.
/// The string views are shared between the two objects.
template <>
MRS_API AudioDeviceConfiguration
FromInterop<AudioDeviceConfiguration, mrsAudioDeviceConfiguration>(
    const mrsAudioDeviceConfiguration& interop);

}  // namespace Microsoft::MixedReality::WebRTC::detail

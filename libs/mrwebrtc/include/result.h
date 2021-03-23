// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "export.h"

namespace Microsoft {
namespace MixedReality {
namespace WebRTC {

/// Result code from an operation, typically used through the interop layer
/// instead of a full-featured Error object.
///
/// Somewhat similar to webrtc::RTCErrorType to avoid pulling it as a dependency
/// in the public API. This also has extra values not found in
/// webrtc::RTCErrorType.
enum class Result : uint32_t {
  /// The operation was successful.
  kSuccess = 0,

  //
  // Generic errors
  //

  /// Unknown internal error.
  /// This is generally the fallback value when no other error code applies.
  kUnknownError = 0x80000000u,

  /// A parameter passed to the API function was invalid.
  kInvalidParameter = 0x80000001u,

  /// The operation cannot be performed in the current state.
  kInvalidOperation = 0x80000002u,

  /// A call was made to an API function on the wrong thread.
  /// This is generally related to platforms with thread affinity like UWP.
  kWrongThread = 0x80000003u,

  /// An object was not found.
  kNotFound = 0x80000004u,

  /// An interop handle referencing a native object instance is invalid,
  /// although the API function was expecting a valid object.
  kInvalidNativeHandle = 0x80000005u,

  /// The API object is not initialized, and cannot as a result perform the
  /// given operation.
  kNotInitialized = 0x80000006u,

  /// The current operation is not supported by the implementation.
  kUnsupported = 0x80000007u,

  /// An argument was passed to the API function with a value out of the
  /// expected range.
  kOutOfRange = 0x80000008u,

  /// The buffer provided by the caller was too small for the operation to
  /// complete successfully.
  kBufferTooSmall = 0x80000009u,

  //
  // Peer connection (0x1xx)
  //

  /// The peer connection is closed, but the current operation requires an open
  /// peer connection.
  kPeerConnectionClosed = 0x80000101u,

  //
  // Data (0x3xx)
  //

  /// The SCTP handshake for data channels encryption was not performed, because
  /// the connection was established before any data channel was added to it.
  /// Due to limitations in the implementation, without SCTP handshake data
  /// channels cannot be used, and therefor applications expecting to use data
  /// channels must open at least a single channel before establishing a peer
  /// connection (calling |CreateOffer()|).
  kSctpNotNegotiated = 0x80000301u,

  /// The specified data channel ID is invalid.
  kInvalidDataChannelId = 0x80000302u,

  //
  // Media (0x4xx)
  //

  /// Some audio-only function was called on a video-only object or vice-versa.
  /// For example, trying to get the local audio track of a video transceiver.
  kInvalidMediaKind = 0x80000401u,
};

}  // namespace WebRTC
}  // namespace MixedReality
}  // namespace Microsoft

using mrsResult = Microsoft::MixedReality::WebRTC::Result;

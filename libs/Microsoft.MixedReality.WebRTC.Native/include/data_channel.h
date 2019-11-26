// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <mutex>

#include "callback.h"
#include "data_channel.h"
#include "str.h"
#include "tracked_object.h"

using mrsDataChannelInteropHandle = void*;

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;

/// A data channel is a bidirectional pipe established between the local and
/// remote peer to carry random blobs of data.
/// The data channel API does not specify the content of the data; instead the
/// user can transmit any data in the form of a raw stream of bytes. All data
/// channels are managed and transported with DTLS-SCTP, and therefore are
/// encrypted. A data channel can be configured on creation to be either or both
/// of:
/// - reliable: data is guaranteed to be transmitted to the remote peer, by
/// re-sending lost packets as many times as needed.
/// - ordered: data is received by the remote peer in the same order as it is
/// sent by the local peer.
class DataChannel : public TrackedObject {
 public:
  /// Data channel state as marshaled through the public API.
  enum class State : int {
    /// The data channel is being connected, but is not yet ready to send nor
    /// received any data.
    kConnecting = 0,

    /// The data channel is ready for read and write operations.
    kOpen = 1,

    /// The data channel is being closed, and cannot send any more data.
    kClosing = 2,

    /// The data channel is closed, and cannot be used again anymore.
    kClosed = 3
  };

  /// Callback fired on newly available data channel data.
  using MessageCallback = Callback<const void*, const uint64_t>;

  /// Callback fired when data buffering changed.
  /// The first parameter indicates the old buffering amount in bytes, the
  /// second one the new value, and the last one indicates the limit in bytes
  /// (buffer capacity). This is important because if the send buffer is full
  /// then any attempt to send data will abruptly close the data channel. See
  /// comment in webrtc::DataChannelInterface::Send() for details. Current
  /// WebRTC implementation has a limit of 16MB for the buffer capacity.
  using BufferingCallback =
      Callback<const uint64_t, const uint64_t, const uint64_t>;

  /// Callback fired when the data channel state changed.
  using StateCallback = Callback</*DataChannelState*/ int, int>;

  /// Get the unique channel identifier.
  [[nodiscard]] MRS_API virtual int id() const = 0;

  /// Get the friendly channel name.
  [[nodiscard]] MRS_API virtual str label() const = 0;

  MRS_API virtual void SetMessageCallback(
      MessageCallback callback) noexcept = 0;
  MRS_API virtual void SetBufferingCallback(
      BufferingCallback callback) noexcept = 0;
  MRS_API virtual void SetStateCallback(StateCallback callback) noexcept = 0;

  /// Get the maximum buffering size, in bytes, before |Send()| stops accepting
  /// data.
  [[nodiscard]] MRS_API virtual size_t GetMaxBufferingSize() const noexcept = 0;

  /// Send a blob of data through the data channel.
  MRS_API virtual bool Send(const void* data, size_t size) noexcept = 0;
};

}  // namespace Microsoft::MixedReality::WebRTC

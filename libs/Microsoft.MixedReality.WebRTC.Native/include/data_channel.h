// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <cstdint>

#include "callback.h"
#include "data_channel.h"

#include "interop_api.h"

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
class DataChannel {
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

  /// Remove the data channel from its parent PeerConnection and close it.
  virtual ~DataChannel();

  /// Get the unique channel identifier.
  [[nodiscard]] int id() const noexcept { return id_; }

  /// Get the friendly channel name.
  [[nodiscard]] std::string label() const { return label_; }

  /// Send a blob of data through the data channel.
  void Send(const void* data, size_t size);

  //
  // Events
  //

  /// Callback invoked when a message is received.
  virtual void OnMessageReceived(const void* /*data*/, size_t /*size*/) {}

  /// Callback invoked when the internal message buffering changed.
  virtual void OnBufferingChanged(size_t /*previous*/,
                                  size_t /*current*/,
                                  size_t /*max_capacity*/) {}

  /// Callback invoked when the data channel state changed.
  virtual void OnStateChanged(State /*state*/) {}

  //
  // Errors
  //

  /// Helper method invoked for each API call, to check that the call was
  /// successful. This default implementation throws an exception depending on
  /// the type of error in |result|.
  virtual void CheckResult(mrsResult result) const { ThrowOnError(result); }

  //
  // Advanced use
  //

  /// Get the handle to the implementation object, to make an API call.
  /// This throws an |InvalidOperationException| if the data channel was closed,
  /// or more generally when the handle is not valid.
  DataChannelHandle GetHandle() const {
    if (!handle_) {
      throw new InvalidOperationException();
    }
    return handle_;
  }

  /// This is invoked automatically by PeerConnection::RemoveDataChannel().
  /// Do not call it manually.
  void OnRemovedFromPeerConnection() noexcept { owner_ = nullptr; }

 private:
  DataChannel(PeerConnection* owner,
              int id,
              std::string_view label,
              bool ordered,
              bool reliable) noexcept;

  static void StaticMessageCallback(void* user_data,
                                    const void* data,
                                    const uint64_t size);
  static void StaticBufferingCallback(void* user_data,
                                      const uint64_t previous,
                                      const uint64_t current,
                                      const uint64_t max_capacity);
  static void StaticStateCallback(void* user_data, int state, int id);

  // See PeerConnection::AddDataChannel()
  friend class PeerConnection;

 private:
  /// PeerConnection object owning this data channel. This is only valid from
  /// creation until the data channel is removed from the peer connection with
  /// RemoveDataChannel(), at which point the data channel is removed from its
  /// parent's collection and |owner_| is set to nullptr.
  PeerConnection* owner_{};

  /// Handle to the implementation object.
  DataChannelHandle handle_{};

  /// Unique ID of the data channel within the current session.
  const int id_;

  ///
  const std::string label_;

  const bool ordered_;
  const bool reliable_;
};

}  // namespace Microsoft::MixedReality::WebRTC

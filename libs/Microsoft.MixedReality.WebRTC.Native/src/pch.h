// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include <Microsoft.MixedReality.WebRTC.h>

/// Exception thrown when an operation cannot be performed because the peer
/// connection it operates on was previously closed.
class ConnectionClosedException : public std::runtime_error {
 public:
  ConnectionClosedException() noexcept
      : std::runtime_error(
            "Cannot perform operation on closed peer connection."){};
};

class NotImplementedException : public std::exception {};

class InvalidOperationException : public std::runtime_error {
 public:
  InvalidOperationException() noexcept
      : std::runtime_error("Invalid operation."){};
};

inline void ThrowOnError(mrsResult result) {
  switch (result) {
    case mrsResult::kSuccess:
      return;

    case mrsResult::kInvalidParameter:
      throw new std::invalid_argument("Invalid argument.");

    case mrsResult::kPeerConnectionClosed:
      throw new ConnectionClosedException();

      // TODO...

    default:
      throw;
  }
}

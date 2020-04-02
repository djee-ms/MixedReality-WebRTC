// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <mrwebrtc/export.h>

namespace Microsoft::MixedReality::WebRTC {

/// Wrapper for a static callback with user data.
/// Usage:
///   void* user_data = [...]
///   Callback<int> cb = {func_ptr, user_data};
///   cb(42); // -> func_ptr(user_data, 42)
///   Callback<float> cb2;
///   cb2(3.4f); // -> safe, does nothing
template <typename... Args>
class Callback {
 public:
  /// Check if the callback has a valid function pointer.
  constexpr explicit operator bool() const noexcept { return callback_; }

  /// Invoke the callback with the given arguments |args|.
  constexpr void operator()(Args... args) const noexcept {
    callback_(std::forward<Args>(args)...);
  }

  static void StaticExec(void* user_data, Args... args) noexcept {
    auto cb = static_cast<callback_type*>(user_data);
    (*cb)(std::forward<Args>(args)...);
  }

  void* GetUserData() const noexcept { return &callback_; }

 private:
  /// Type of the raw callback function.
  /// The first parameter is the opaque user data pointer.
  using raw_callback_type = void(MRS_CALL*)(void*, Args...);

  /// Type of the library callback function.
  using callback_type = std::function<void(Args...)>;

  /// Callback.
  callback_type callback_;

  /// Pointer to the raw function to invoke.
  raw_callback_type raw_callback_{};

  /// User-provided opaque pointer passed as first argument to the raw function.
  void* user_data_{};
};

/// Same as |Callback|, with a return value.
template <typename Ret, typename... Args>
struct RetCallback {
  /// Type of the return value.
  using return_type = Ret;

  /// Type of the raw callback function.
  /// The first parameter is the opaque user data pointer.
  using callback_type = return_type(MRS_CALL*)(void*, Args...);

  /// Pointer to the raw function to invoke.
  callback_type callback_{};

  /// User-provided opaque pointer passed as first argument to the raw function.
  void* user_data_{};

  /// Check if the callback has a valid function pointer.
  constexpr explicit operator bool() const noexcept {
    return (callback_ != nullptr);
  }

  /// Invoke the callback with the given arguments |args|.
  constexpr return_type operator()(Args... args) const noexcept {
    if (callback_ != nullptr) {
      return (*callback_)(user_data_, std::forward<Args>(args)...);
    }
    return return_type{};
  }
};

}  // namespace Microsoft::MixedReality::WebRTC

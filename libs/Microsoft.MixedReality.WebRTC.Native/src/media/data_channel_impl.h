// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <mutex>

#include "api/datachannelinterface.h"
#include "rtc_base/thread_annotations.h"

#include "data_channel.h"

namespace Microsoft::MixedReality::WebRTC::impl {

/// Private implementation of DataChannel.
class DataChannelImpl : public DataChannel, public webrtc::DataChannelObserver {
 public:
  DataChannelImpl(
      PeerConnection* owner,
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel,
      mrsDataChannelInteropHandle interop_handle = nullptr) noexcept;

  /// Remove the data channel from its parent PeerConnection and close it.
  ~DataChannelImpl() override;

  /// Get the tracked object name, for debugging. This currently uses the data
  /// channel friendly name from |label()|.
  [[nodiscard]] str GetName() const override {
    return str(data_channel_->label());
  }

  /// Get the unique channel identifier.
  [[nodiscard]] int id() const override { return data_channel_->id(); }

  /// Get the friendly channel name.
  [[nodiscard]] MRS_API str label() const override;

  void SetMessageCallback(MessageCallback callback) noexcept override;
  void SetBufferingCallback(BufferingCallback callback) noexcept override;
  void SetStateCallback(StateCallback callback) noexcept override;

  /// Get the maximum buffering size, in bytes, before |Send()| stops accepting
  /// data.
  [[nodiscard]] MRS_API size_t GetMaxBufferingSize() const noexcept override;

  /// Send a blob of data through the data channel.
  MRS_API bool Send(const void* data, size_t size) noexcept override;

  //
  // Advanced use
  //

  [[nodiscard]] webrtc::DataChannelInterface* impl() const {
    return data_channel_.get();
  }

  mrsDataChannelInteropHandle GetInteropHandle() const noexcept {
    return interop_handle_;
  }

  /// This is invoked automatically by PeerConnection::RemoveDataChannel().
  /// Do not call it manually.
  void OnRemovedFromPeerConnection() noexcept { owner_ = nullptr; }

 protected:
  // DataChannelObserver interface

  // The data channel state have changed.
  void OnStateChange() noexcept override;
  //  A data buffer was successfully received.
  void OnMessage(const webrtc::DataBuffer& buffer) noexcept override;
  // The data channel's buffered_amount has changed.
  void OnBufferedAmountChange(uint64_t previous_amount) noexcept override;

 private:
  /// PeerConnection object owning this data channel. This is only valid from
  /// creation until the data channel is removed from the peer connection with
  /// RemoveDataChannel(), at which point the data channel is removed from its
  /// parent's collection and |owner_| is set to nullptr.
  PeerConnection* owner_{};

  /// Underlying core implementation.
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;

  MessageCallback message_callback_ RTC_GUARDED_BY(mutex_);
  BufferingCallback buffering_callback_ RTC_GUARDED_BY(mutex_);
  StateCallback state_callback_ RTC_GUARDED_BY(mutex_);
  std::mutex mutex_;

  /// Optional interop handle, if associated with an interop wrapper.
  mrsDataChannelInteropHandle interop_handle_{};
};

}  // namespace Microsoft::MixedReality::WebRTC::impl

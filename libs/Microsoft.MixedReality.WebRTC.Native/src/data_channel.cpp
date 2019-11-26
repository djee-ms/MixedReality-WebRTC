// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "media/data_channel_impl.h"
#include "peer_connection.h"
#include "interop/global_factory.h"

namespace {

using namespace Microsoft::MixedReality::WebRTC;

using RtcDataState = webrtc::DataChannelInterface::DataState;
using ApiDataState = DataChannel::State;

inline ApiDataState apiStateFromRtcState(RtcDataState rtcState) {
  // API values have been chosen to match the current WebRTC values. If the
  // later change, this helper must be updated, as API values cannot change.
  static_assert((int)RtcDataState::kOpen == (int)ApiDataState::kOpen);
  static_assert((int)RtcDataState::kConnecting ==
                (int)ApiDataState::kConnecting);
  static_assert((int)RtcDataState::kClosing == (int)ApiDataState::kClosing);
  static_assert((int)RtcDataState::kClosed == (int)ApiDataState::kClosed);
  return (ApiDataState)rtcState;
}
}  // namespace

namespace Microsoft::MixedReality::WebRTC::impl {

DataChannelImpl::DataChannelImpl(
    PeerConnection* owner,
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel,
    mrsDataChannelInteropHandle interop_handle) noexcept
    : owner_(owner),
      data_channel_(std::move(data_channel)),
      interop_handle_(interop_handle) {
  RTC_CHECK(owner_);
  data_channel_->RegisterObserver(this);
  GlobalFactory::Instance()->AddObject(ObjectType::kDataChannel, this);
}

DataChannelImpl::~DataChannelImpl() {
  data_channel_->UnregisterObserver();
  if (owner_) {
    owner_->RemoveDataChannel(*this);
  }
  RTC_CHECK(!owner_);
  GlobalFactory::Instance()->RemoveObject(ObjectType::kDataChannel, this);
}

str DataChannelImpl::label() const {
  return str{data_channel_->label()};
}

void DataChannelImpl::SetMessageCallback(MessageCallback callback) noexcept {
  auto lock = std::scoped_lock{mutex_};
  message_callback_ = callback;
}

void DataChannelImpl::SetBufferingCallback(
    BufferingCallback callback) noexcept {
  auto lock = std::scoped_lock{mutex_};
  buffering_callback_ = callback;
}

void DataChannelImpl::SetStateCallback(StateCallback callback) noexcept {
  auto lock = std::scoped_lock{mutex_};
  state_callback_ = callback;
}

size_t DataChannelImpl::GetMaxBufferingSize() const noexcept {
  // See BufferingCallback; current WebRTC implementation has a limit of 16MB
  // for the internal data track buffer capacity.
  static constexpr size_t kMaxBufferingSize = 0x1000000uLL;  // 16 MB
  return kMaxBufferingSize;
}

bool DataChannelImpl::Send(const void* data, size_t size) noexcept {
  if (data_channel_->buffered_amount() + size > GetMaxBufferingSize()) {
    return false;
  }
  rtc::CopyOnWriteBuffer bufferStorage((const char*)data, size);
  webrtc::DataBuffer buffer(bufferStorage, /* binary = */ true);
  return data_channel_->Send(buffer);
}

void DataChannelImpl::OnStateChange() noexcept {
  const webrtc::DataChannelInterface::DataState state = data_channel_->state();
  switch (state) {
    case webrtc::DataChannelInterface::DataState::kOpen:
      // Negotiated (out-of-band) data channels never generate an
      // OnDataChannel() message, so simulate it for the DataChannelAdded event
      // to be consistent.
      if (data_channel_->negotiated()) {
        owner_->OnDataChannelAdded(*this);
      }
      break;
  }

  // Invoke the StateChanged event
  {
    auto lock = std::scoped_lock{mutex_};
    if (state_callback_) {
      auto apiState = apiStateFromRtcState(state);
      state_callback_((int)apiState, data_channel_->id());
    }
  }
}

void DataChannelImpl::OnMessage(const webrtc::DataBuffer& buffer) noexcept {
  auto lock = std::scoped_lock{mutex_};
  if (message_callback_) {
    message_callback_(buffer.data.data(), buffer.data.size());
  }
}

void DataChannelImpl::OnBufferedAmountChange(
    uint64_t previous_amount) noexcept {
  auto lock = std::scoped_lock{mutex_};
  if (buffering_callback_) {
    uint64_t current_amount = data_channel_->buffered_amount();
    constexpr uint64_t max_capacity =
        0x1000000;  // 16MB, see DataChannelInterface
    buffering_callback_(previous_amount, current_amount, max_capacity);
  }
}

}  // namespace Microsoft::MixedReality::WebRTC::impl

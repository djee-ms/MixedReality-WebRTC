// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "data_channel.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

DataChannel::DataChannel(PeerConnection* owner,
                         int id,
                         std::string_view label,
                         bool ordered,
                         bool reliable) noexcept
    : owner_(owner),
      id_(id),
      label_(label),
      ordered_(ordered),
      reliable_(reliable) {
  assert(owner_);
}

DataChannel::~DataChannel() {
  if (owner_) {
    owner_->RemoveDataChannel(*this);
  }
  assert(!owner_);
}

void DataChannel::Send(const void* data, size_t size) {
  CheckResult(mrsDataChannelSendMessage(GetHandle(), data,
                                        static_cast<uint64_t>(size)));
}

void DataChannel::StaticMessageCallback(void* user_data,
                                        const void* data,
                                        const uint64_t size) {
  auto ptr = static_cast<DataChannel*>(user_data);
  ptr->OnMessageReceived(data, static_cast<size_t>(size));
}

void DataChannel::StaticBufferingCallback(void* user_data,
                                          const uint64_t previous,
                                          const uint64_t current,
                                          const uint64_t max_capacity) {
  auto ptr = static_cast<DataChannel*>(user_data);
  ptr->OnBufferingChanged(static_cast<size_t>(previous),
                          static_cast<size_t>(current),
                          static_cast<size_t>(max_capacity));
}
void DataChannel::StaticStateCallback(void* user_data, int state, int id) {
  auto ptr = static_cast<DataChannel*>(user_data);
  assert(ptr->id() == id);
  ptr->OnStateChanged(static_cast<State>(state));
}

}  // namespace Microsoft::MixedReality::WebRTC

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"
#include "media/audio_track_source.h"

namespace Microsoft {
namespace MixedReality {
namespace WebRTC {

AudioSourceAdapter::AudioSourceAdapter(
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source)
    : source_(std::move(source)), state_(source->state()) {}

void AudioSourceAdapter::RegisterObserver(webrtc::ObserverInterface* observer) {
  observer_ = observer;
}

void AudioSourceAdapter::UnregisterObserver(
    webrtc::ObserverInterface* observer) {
  RTC_DCHECK_EQ(observer_, observer);
  observer_ = nullptr;
}

void AudioSourceAdapter::AddSink(webrtc::AudioTrackSinkInterface* sink) {
  sinks_.push_back(sink);
}

void AudioSourceAdapter::RemoveSink(webrtc::AudioTrackSinkInterface* sink) {
  auto it = std::find(sinks_.begin(), sinks_.end(), sink);
  if (it != sinks_.end()) {
    sinks_.erase(it);
  }
}

AudioTrackSource::AudioTrackSource(
    RefPtr<GlobalFactory> global_factory,
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source) noexcept
    : TrackedObject(std::move(global_factory), ObjectType::kAudioTrackSource),
      source_(std::move(source)) {
  RTC_CHECK(source_);
}

AudioTrackSource::~AudioTrackSource() {}

}  // namespace WebRTC
}  // namespace MixedReality
}  // namespace Microsoft

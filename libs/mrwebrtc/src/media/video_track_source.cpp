// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"
#include "media/video_track_source.h"

namespace Microsoft {
namespace MixedReality {
namespace WebRTC {

VideoSourceAdapter::VideoSourceAdapter(
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source)
    : source_(std::move(source)), state_(source->state()) {}

void VideoSourceAdapter::RegisterObserver(webrtc::ObserverInterface* observer) {
  observer_ = observer;
}

void VideoSourceAdapter::UnregisterObserver(
    webrtc::ObserverInterface* observer) {
  RTC_DCHECK_EQ(observer_, observer);
  observer_ = nullptr;
}

//void VideoSourceAdapter::AddSink(webrtc::AudioTrackSinkInterface* sink) {
//  sinks_.push_back(sink);
//}
//
//void VideoSourceAdapter::RemoveSink(webrtc::AudioTrackSinkInterface* sink) {
//  auto it = std::find(sinks_.begin(), sinks_.end(), sink);
//  if (it != sinks_.end()) {
//    sinks_.erase(it);
//  }
//}

VideoTrackSource::VideoTrackSource(
    RefPtr<GlobalFactory> global_factory,
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source) noexcept
    : TrackedObject(std::move(global_factory), ObjectType::kVideoTrackSource),
      source_(std::move(source)) {
  RTC_CHECK(source_);
}

VideoTrackSource::~VideoTrackSource() {}

}  // namespace WebRTC
}  // namespace MixedReality
}  // namespace Microsoft

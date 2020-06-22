// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"
#include "media/external_audio_track_source.h"

namespace {

enum {
  /// Request a new audio frame from the source.
  MSG_REQUEST_FRAME
};

}  // namespace

namespace Microsoft {
namespace MixedReality {
namespace WebRTC {

constexpr const size_t kMaxPendingRequestCount = 64;

void detail::CustomAudioTrackSourceAdapter::AddSink(
    webrtc::AudioTrackSinkInterface* sink) {
  std::lock_guard<std::mutex> lock(sink_mutex_);
  sinks_.push_back(sink);
}

void detail::CustomAudioTrackSourceAdapter::RemoveSink(
    webrtc::AudioTrackSinkInterface* sink) {
  std::lock_guard<std::mutex> lock(sink_mutex_);
  auto it = std::find(sinks_.begin(), sinks_.end(), sink);
  if (it != sinks_.end()) {
    sinks_.erase(it);
  }
}

void detail::CustomAudioTrackSourceAdapter::DispatchFrame(
    const AudioFrame& frame_view) const {
  std::lock_guard<std::mutex> lock(sink_mutex_);
  for (auto&& sink : sinks_) {
    sink->OnData(frame_view.data_, frame_view.bits_per_sample_,
                 frame_view.sampling_rate_hz_, frame_view.channel_count_,
                 frame_view.sample_count_);
  }
}

ExternalAudioTrackSource::ExternalAudioTrackSource(
    RefPtr<GlobalFactory> global_factory,
    RefPtr<ExternalAudioSource> audio_source,
    rtc::scoped_refptr<detail::CustomAudioTrackSourceAdapter> source)
    : AudioTrackSource(std::move(global_factory),
                       ObjectType::kExternalAudioTrackSource,
                       source),
      audio_source_(std::move(audio_source)),
      capture_thread_(rtc::Thread::Create()) {
  capture_thread_->SetName("ExternalAudioTrackSource capture thread", this);
}

ExternalAudioTrackSource::~ExternalAudioTrackSource() {
  StopCapture();
}

void ExternalAudioTrackSource::FinishCreation() {
  StartCapture();
}

void ExternalAudioTrackSource::StartCapture() {
  // Check if |Shutdown()| was called, in which case the source cannot restart.
  if (!source_) {
    return;
  }

  RTC_LOG(LS_INFO) << "Starting capture for external audio track source "
                   << GetName().c_str();

  // Start capture thread
  GetSourceImpl()->SetState(SourceState::kLive);
  pending_requests_.clear();
  capture_thread_->Start();

  // Schedule first frame request for 10ms from now
  int64_t now = rtc::TimeMillis();
  capture_thread_->PostAt(RTC_FROM_HERE, now + 10, this, MSG_REQUEST_FRAME);
}

Result ExternalAudioTrackSource::CompleteRequest(uint32_t request_id,
                                                 int64_t timestamp_ms,
                                                 const AudioFrame& frame_view) {
  // Validate pending request ID and retrieve frame timestamp
  int64_t timestamp_ms_original = -1;
  {
    rtc::CritScope lock(&request_lock_);
    for (auto it = pending_requests_.begin(); it != pending_requests_.end();
         ++it) {
      if (it->first == request_id) {
        timestamp_ms_original = it->second;
        // Remove outdated requests, including current one
        ++it;
        pending_requests_.erase(pending_requests_.begin(), it);
        break;
      }
    }
    if (timestamp_ms_original < 0) {
      return Result::kInvalidParameter;
    }
  }

  // Apply user override if any
  if (timestamp_ms != timestamp_ms_original) {
    timestamp_ms = timestamp_ms_original;
  }

  // Dispatch the audio frame to the sinks
  GetSourceImpl()->DispatchFrame(frame_view);
  return Result::kSuccess;
}

void ExternalAudioTrackSource::StopCapture() {
  detail::CustomAudioTrackSourceAdapter* const src = GetSourceImpl();
  if (!src) {  // already stopped
    return;
  }
  if (src->state() != SourceState::kEnded) {
    RTC_LOG(LS_INFO) << "Stopping capture for external audio track source "
                     << GetName().c_str();
    capture_thread_->Stop();
    src->SetState(SourceState::kEnded);
  }
  pending_requests_.clear();
}

void ExternalAudioTrackSource::Shutdown() noexcept {
  StopCapture();
  source_ = nullptr;
}

// Note - This is called on the capture thread only.
void ExternalAudioTrackSource::OnMessage(rtc::Message* message) {
  switch (message->message_id) {
    case MSG_REQUEST_FRAME:
      const int64_t now = rtc::TimeMillis();

      // Request a frame from the external audio source
      uint32_t request_id = 0;
      {
        rtc::CritScope lock(&request_lock_);
        // Discard an old request if no space available. This allows restarting
        // after a long delay, otherwise skipping the request generally also
        // prevent the user from calling CompleteFrame() to make some space for
        // more. The queue is still useful for just-in-time or short delays.
        if (pending_requests_.size() >= kMaxPendingRequestCount) {
          pending_requests_.erase(pending_requests_.begin());
        }
        request_id = next_request_id_++;
        pending_requests_.emplace_back(request_id, now);
      }
      AudioFrameRequest request{*this, now, request_id};
      audio_source_->FrameRequested(request);

      // Schedule a new request for 30ms from now
      //< TODO - this is unreliable and prone to drifting; figure out something
      // better
      capture_thread_->PostAt(RTC_FROM_HERE, now + 30, this, MSG_REQUEST_FRAME);
      break;
  }
}

RefPtr<ExternalAudioTrackSource> ExternalAudioTrackSource::create(
    RefPtr<GlobalFactory> global_factory,
    RefPtr<ExternalAudioSource> audio_source) {
  auto source = new ExternalAudioTrackSource(
      std::move(global_factory), std::move(audio_source),
      new rtc::RefCountedObject<detail::CustomAudioTrackSourceAdapter>());
  // Note: Audio track sources always start already capturing; there is no
  // start/stop mechanism at the track level in WebRTC. A source is either being
  // initialized, or is already live. However because of wrappers and interop
  // this step is delayed until |FinishCreation()| is called by the wrapper.
  return source;
}

Result AudioFrameRequest::CompleteRequest(const AudioFrame& frame_view) {
  auto impl = static_cast<ExternalAudioTrackSource*>(&track_source_);
  return impl->CompleteRequest(request_id_, timestamp_ms_, frame_view);
}

}  // namespace WebRTC
}  // namespace MixedReality
}  // namespace Microsoft

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "audio_frame.h"
#include "audio_track_source.h"
#include "external_audio_track_source_interop.h"
#include "mrs_errors.h"
#include "refptr.h"
#include "tracked_object.h"

#pragma warning(push, 2)
#pragma warning(disable : 4100)
#include "pc/localaudiosource.h"
#pragma warning(pop)

namespace Microsoft {
namespace MixedReality {
namespace WebRTC {

class ExternalAudioTrackSource;

namespace detail {

/// Adapter to bridge an audio track source to the underlying core
/// implementation.
class CustomAudioTrackSourceAdapter
    // Note: this is a very ugly workaround for the fact
    // AudioRtpSender::SetAudioSend() breaks abstraction and systematically
    // casts any local audio source to webrtc::LocalAudioSource to retrieve the
    // audio options. Luckily the webrtc::LocalAudioSource class does almost
    // nothing, so can derive from it instead of the proper interface.
    : public webrtc::LocalAudioSource /*webrtc::AudioSourceInterface*/ {
 public:
  // AudioSourceInterface
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override;
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override;

  // MediaSourceInterface
  SourceState state() const override { return state_; }
  bool remote() const override { return false; }

  // NotifierInterface
  void RegisterObserver(webrtc::ObserverInterface* /*observer*/) override {}
  void UnregisterObserver(webrtc::ObserverInterface* /*observer*/) override {}

  void SetState(SourceState state) { state_ = state; }
  void DispatchFrame(const AudioFrame& frame_view) const;

 protected:
  SourceState state_ = SourceState::kInitializing;
  mutable std::mutex sink_mutex_;
  std::vector<webrtc::AudioTrackSinkInterface*> sinks_;
};

}  // namespace detail

/// Frame request for an external audio source producing audio frames encoded in
/// PCM format.
struct AudioFrameRequest {
  /// Audio track source the request is related to.
  ExternalAudioTrackSource& track_source_;

  /// Audio frame timestamp, in milliseconds.
  std::int64_t timestamp_ms_;

  /// Unique identifier of the request.
  const std::uint32_t request_id_;

  /// Complete the request by making the track source consume the given audio
  /// frame and have it deliver the frame to all its audio tracks.
  Result CompleteRequest(const AudioFrame& frame_view);
};

/// Custom audio source producing audio frames encoded in PCM format.
class ExternalAudioSource : public RefCountedBase {
 public:
  /// Produce a audio frame for a request initiated by an external track source.
  ///
  /// This callback is invoked automatically by the track source whenever a new
  /// audio frame is needed (pull model). The custom audio source implementation
  /// must either return an error, or produce a new audio frame and call the
  /// |CompleteRequest()| request on the |frame_request| object.
  virtual Result FrameRequested(AudioFrameRequest& frame_request) = 0;
};

/// Audio track source acting as an adapter for an external source of raw
/// frames.
class ExternalAudioTrackSource : public AudioTrackSource,
                                 public rtc::MessageHandler {
 public:
  using SourceState = webrtc::MediaSourceInterface::SourceState;

  /// Helper to create an external audio track source from a custom PCM audio
  /// frame request callback.
  static RefPtr<ExternalAudioTrackSource> create(
      RefPtr<GlobalFactory> global_factory,
      RefPtr<ExternalAudioSource> audio_source);

  ~ExternalAudioTrackSource() override;

  /// Finish the creation of the audio track source, and start capturing.
  /// See |mrsExternalAudioTrackSourceFinishCreation()| for details.
  void FinishCreation();

  /// Start the audio capture. This will begin to produce audio frames and start
  /// calling the audio frame callback.
  void StartCapture();

  /// Complete a given audio frame request with the provided PCM frame.
  Result CompleteRequest(uint32_t request_id,
                         int64_t timestamp_ms,
                         const AudioFrame& frame);

  /// Stop the audio capture. This will stop producing audio frames.
  void StopCapture();

  /// Shutdown the source and release the buffer adapter and its callback.
  void Shutdown() noexcept;

 protected:
  ExternalAudioTrackSource(
      RefPtr<GlobalFactory> global_factory,
      RefPtr<ExternalAudioSource> audio_source,
      rtc::scoped_refptr<detail::CustomAudioTrackSourceAdapter> source);

  void OnMessage(rtc::Message* message) override;

  detail::CustomAudioTrackSourceAdapter* GetSourceImpl() const {
    return (detail::CustomAudioTrackSourceAdapter*)source_.get();
  }

  /// External audio source providing the frames.
  RefPtr<ExternalAudioSource> audio_source_;

  std::unique_ptr<rtc::Thread> capture_thread_;

  /// Collection of pending frame requests
  std::deque<std::pair<uint32_t, int64_t>> pending_requests_
      RTC_GUARDED_BY(request_lock_);  //< TODO : circular buffer to avoid alloc

  /// Next available ID for a frame request.
  uint32_t next_request_id_ RTC_GUARDED_BY(request_lock_){};

  /// Lock for frame requests.
  rtc::CriticalSection request_lock_;
};

namespace detail {

/// Create an external audio track source wrapping the given interop callback.
RefPtr<ExternalAudioTrackSource> ExternalAudioTrackSourceCreate(
    RefPtr<GlobalFactory> global_factory,
    mrsRequestExternalAudioFrameCallback callback,
    void* user_data);

}  // namespace detail

}  // namespace WebRTC
}  // namespace MixedReality
}  // namespace Microsoft

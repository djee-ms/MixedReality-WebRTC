// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "external_video_track_source_interop.h"
#include "mrs_errors.h"
#include "refptr.h"
#include "tracked_object.h"
#include "video_frame.h"

#include "api/mediastreaminterface.h"

namespace Microsoft {
namespace MixedReality {
namespace WebRTC {

class AudioTrackSource;

/// Adapter for a local audio source backing one or more local audio tracks.
class AudioSourceAdapter : public webrtc::AudioSourceInterface {
 public:
  AudioSourceAdapter(rtc::scoped_refptr<webrtc::AudioSourceInterface> source);

  //
  // NotifierInterface
  //

  void RegisterObserver(webrtc::ObserverInterface* observer) override;
  void UnregisterObserver(webrtc::ObserverInterface* observer) override;

  //
  // MediaSourceInterface
  //

  SourceState state() const override { return state_; }
  bool remote() const override { return false; }

  //
  // AudioSourceInterface
  //

  // Sets the volume of the source. |volume| is in  the range of [0, 10].
  // TODO(tommi): This method should be on the track and ideally volume should
  // be applied in the track in a way that does not affect clones of the track.
  void SetVolume(double /*volume*/) override {}

  // Registers/unregisters observers to the audio source.
  void RegisterAudioObserver(AudioObserver* /*observer*/) override {}
  void UnregisterAudioObserver(AudioObserver* /*observer*/) override {}

  // TODO(tommi): Make pure virtual.
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override;
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override;

 protected:
  rtc::scoped_refptr<webrtc::AudioSourceInterface> source_;
  std::vector<webrtc::AudioTrackSinkInterface*> sinks_;
  SourceState state_{SourceState::kEnded};
  webrtc::ObserverInterface* observer_{nullptr};
  AudioObserver* audio_observer_{nullptr};
};

/// Base class for an audio track source acting as a frame source for one or
/// more audio tracks.
class AudioTrackSource : public TrackedObject {
 public:
  ///// Helper to create an audio track source from a custom audio frame request
  ///// callback.
  // static RefPtr<AudioTrackSource> createFromCustom(
  //    RefPtr<GlobalFactory> global_factory,
  //    RefPtr<CustomAudioSource> audio_source);

  AudioTrackSource(
      RefPtr<GlobalFactory> global_factory,
      rtc::scoped_refptr<webrtc::AudioSourceInterface> source) noexcept;
  ~AudioTrackSource() override;

  void SetName(absl::string_view name) {
    name_.assign(name.data(), name.size());
  }

  /// Get the name of the audio track source.
  std::string GetName() const noexcept override { return name_; }

  inline rtc::scoped_refptr<webrtc::AudioSourceInterface> impl() const
      noexcept {
    return source_;
  }

 protected:
  rtc::scoped_refptr<webrtc::AudioSourceInterface> source_;

  std::string name_;
};

namespace detail {

//
// Helpers
//

///// Create a custom audio track source wrapping the given interop callback.
// RefPtr<AudioTrackSource> AudioTrackSourceCreateFromCustom(
//    RefPtr<GlobalFactory> global_factory,
//    mrsRequestCustomAudioFrameCallback callback,
//    void* user_data);

}  // namespace detail

}  // namespace WebRTC
}  // namespace MixedReality
}  // namespace Microsoft

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <mutex>
#include <functional>

#include <mrwebrtcxx/callback.h>
#include <mrwebrtcxx/audio_frame.h>

namespace Microsoft::MixedReality::WebRTC {

/// Callback fired on newly available audio frame.
using AudioFrameReadyCallback = std::function<const AudioFrame&>;

/// Audio frame observer to get notified of newly available audio frames.
class AudioFrameObserver {
 public:
  void SetCallback(AudioFrameReadyCallback callback) noexcept;

 protected:
  // AudioTrackSinkInterface interface
  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames) noexcept override;

 private:
  AudioFrameReadyCallback callback_ MRS_GUARDED_BY(mutex_);
  std::mutex mutex_;
};

}  // namespace Microsoft::MixedReality::WebRTC

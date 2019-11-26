// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <mutex>

#include "api/mediastreaminterface.h"

#include "audio_frame.h"

namespace Microsoft::MixedReality::WebRTC {

/// Video frame observer to get notified of newly available video frames.
class AudioFrameObserver : public webrtc::AudioTrackSinkInterface {
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
  AudioFrameReadyCallback callback_;
  std::mutex mutex_;
};

}  // namespace Microsoft::MixedReality::WebRTC

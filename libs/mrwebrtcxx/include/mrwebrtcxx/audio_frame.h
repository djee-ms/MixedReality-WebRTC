// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <cstdint>

namespace Microsoft::MixedReality::WebRTC {

/// View over an existing buffer representing an audio frame, in the sense
/// of a single group of contiguous audio data.
class AudioFrame {
 public:
  /// Get a pointer to the raw audio frame data.
  constexpr const void* data() const { return data_; }

  /// Get the size in bytes of the raw audio frame data.
  constexpr size_t size() const {
    return ((size_t)bits_per_sample_ / 8) * channel_count_ * sample_count_;
  }

  /// Pointer to the raw contiguous memory block holding the audio data in
  /// channel interleaved format. The length of the buffer is at least
  /// (|bits_per_sample_| / 8 * |channel_count_| * |sample_count_|) bytes.
  const void* data_{nullptr};

  /// Number of bits per sample, often 8 or 16, for a single channel.
  std::uint16_t bits_per_sample_{0};

  /// Number of interleaved channels in a single audio sample.
  std::uint16_t channel_count_{0};

  /// Sampling rate, in Hertz (number of samples per second).
  std::uint32_t sampling_rate_hz_{0};

  /// Number of consecutive samples. The frame duration is given by the ratio
  /// |sample_count_| / |sampling_rate_hz_|.
  std::uint32_t sample_count_{0};
};

}  // namespace Microsoft::MixedReality::WebRTC

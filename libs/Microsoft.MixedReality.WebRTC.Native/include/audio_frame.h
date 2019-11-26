// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "callback.h"

namespace Microsoft::MixedReality::WebRTC {

/// Callback fired on newly available audio frame.
/// The callback parameters are:
/// - Audio data buffer pointer.
/// - Number of bits per sample.
/// - Sampling rate, in Hz.
/// - Number of channels.
/// - Number of consecutive audio frames in the buffer.
using AudioFrameReadyCallback = Callback<const void*,
                                         const uint32_t,
                                         const uint32_t,
                                         const uint32_t,
                                         const uint32_t>;

}  // namespace Microsoft::MixedReality::WebRTC

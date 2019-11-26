// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "callback.h"

namespace Microsoft::MixedReality::WebRTC {

/// Callback fired on newly available video frame, encoded as I420.
/// The first 4 pointers are buffers for the YUVA planes (in that order),
/// followed by the 4 byte strides, and the width and height of the frame.
using I420AFrameReadyCallback = Callback<const void*,
                                         const void*,
                                         const void*,
                                         const void*,
                                         int,
                                         int,
                                         int,
                                         int,
                                         int,
                                         int>;

/// Callback fired on newly available video frame, encoded as ARGB.
/// The first parameters are the buffer pointer and byte stride of the
/// packed ARGB data, followed by the width and height of the frame.
using ARGBFrameReadyCallback = Callback<const void*, int, int, int>;

}  // namespace Microsoft::MixedReality::WebRTC

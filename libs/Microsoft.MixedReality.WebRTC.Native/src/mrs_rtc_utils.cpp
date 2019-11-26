// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/interop_api.h"
#include "mrs_rtc_utils.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC::detail {

template <>
VideoDeviceConfiguration
FromInterop<VideoDeviceConfiguration, mrsVideoDeviceConfiguration>(
    const mrsVideoDeviceConfiguration& interop) {
  VideoDeviceConfiguration native;
  native.video_device_id = interop.video_profile_id;
  native.video_profile_id = interop.video_profile_id;
  native.video_profile_kind = interop.video_profile_kind;
  if (interop.width > 0)
    native.width = interop.width;
  if (interop.height > 0)
    native.height = interop.height;
  if (interop.framerate > 0)
    native.framerate = interop.framerate;
  native.enable_mrc = (interop.enable_mrc != mrsBool::kFalse);
  native.enable_mrc_recording_indicator =
      (interop.enable_mrc_recording_indicator != mrsBool::kFalse);
  return native;
}

template <>
AudioDeviceConfiguration
FromInterop<AudioDeviceConfiguration, mrsAudioDeviceConfiguration>(
    const mrsAudioDeviceConfiguration& interop) {
  AudioDeviceConfiguration native;
  if (interop.echo_cancellation != mrsOptBool::kUnspecified) {
    native.echo_cancellation = (interop.echo_cancellation == mrsOptBool::kTrue);
  }
  if (interop.auto_gain_control != mrsOptBool::kUnspecified) {
    native.auto_gain_control = (interop.auto_gain_control == mrsOptBool::kTrue);
  }
  if (interop.noise_suppression != mrsOptBool::kUnspecified) {
    native.noise_suppression = (interop.noise_suppression == mrsOptBool::kTrue);
  }
  if (interop.highpass_filter != mrsOptBool::kUnspecified) {
    native.highpass_filter = (interop.highpass_filter == mrsOptBool::kTrue);
  }
  if (interop.typing_detection != mrsOptBool::kUnspecified) {
    native.typing_detection = (interop.typing_detection == mrsOptBool::kTrue);
  }
  if (interop.aecm_generate_comfort_noise != mrsOptBool::kUnspecified) {
    native.aecm_generate_comfort_noise =
        (interop.aecm_generate_comfort_noise == mrsOptBool::kTrue);
  }
  return native;
}

}  // namespace Microsoft::MixedReality::WebRTC::detail

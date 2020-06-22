// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "interop_api.h"

extern "C" {

/// Create a custom audio track source external to the implementation. This
/// allows feeding into WebRTC frames from any source, including generated or
/// synthetic frames, for example for testing. The frame is provided from a
/// callback as a PCM-encoded buffer. This returns a handle to a newly
/// allocated object, which must be released once not used anymore with
/// |mrsRefCountedObjectRemoveRef()|.
MRS_API mrsResult MRS_CALL mrsExternalAudioTrackSourceCreateFromCallback(
    mrsRequestExternalAudioFrameCallback callback,
    void* user_data,
    mrsExternalAudioTrackSourceHandle* source_handle_out) noexcept;

/// Callback from the wrapper layer indicating that the wrapper has finished
/// creation, and it is safe to start sending frame requests to it. This needs
/// to be called after |mrsExternalAudioTrackSourceCreateFromCallback()| to
/// finish the creation of the audio track source and allow it to start
/// capturing.
MRS_API void MRS_CALL mrsExternalAudioTrackSourceFinishCreation(
    mrsExternalAudioTrackSourceHandle source_handle) noexcept;

/// Complete a audio frame request with a provided audio frame.
MRS_API mrsResult MRS_CALL mrsExternalAudioTrackSourceCompleteFrameRequest(
    mrsExternalAudioTrackSourceHandle handle,
    uint32_t request_id,
    int64_t timestamp_ms,
    const mrsAudioFrame* frame_view) noexcept;

/// Irreversibly stop the audio source frame production and shutdown the audio
/// source.
MRS_API void MRS_CALL mrsExternalAudioTrackSourceShutdown(
    mrsExternalAudioTrackSourceHandle handle) noexcept;

}  // extern "C"

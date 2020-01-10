// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "export.h"
#include "interop/interop_api.h"

extern "C" {

/// Add a reference to the native object associated with the given handle.
MRS_API void MRS_CALL
mrsAudioTransceiverAddRef(AudioTransceiverHandle handle) noexcept;

/// Remove a reference from the native object associated with the given handle.
MRS_API void MRS_CALL
mrsAudioTransceiverRemoveRef(AudioTransceiverHandle handle) noexcept;

/// Get the local audio track associated with this transceiver, if any.
MRS_API mrsResult MRS_CALL mrsAudioTransceiverGetLocalTrack(
    AudioTransceiverHandle transceiver_handle,
    LocalAudioTrackHandle* track_handle_out) noexcept;

/// Get the remote audio track associated with this transceiver, if any.
MRS_API mrsResult MRS_CALL mrsAudioTransceiverGetRemoteTrack(
    AudioTransceiverHandle transceiver_handle,
    RemoteAudioTrackHandle* track_handle_out) noexcept;

}  // extern "C"

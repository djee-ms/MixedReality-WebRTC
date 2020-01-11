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

/// Set the local audio track associated with this transceiver. This new track
/// replaces the existing one, if any. This doesn't require any SDP
/// renegotiation.
MRS_API mrsResult MRS_CALL
mrsAudioTransceiverSetLocalTrack(AudioTransceiverHandle transceiver_handle,
                                 LocalAudioTrackHandle track_handle) noexcept;

/// Get the local audio track associated with this transceiver, if any.
/// The returned handle holds a reference to the video transceiver, which must
/// be released with |mrsLocalAudioTrackRemoveRef()| once not needed anymore.
MRS_API mrsResult MRS_CALL mrsAudioTransceiverGetLocalTrack(
    AudioTransceiverHandle transceiver_handle,
    LocalAudioTrackHandle* track_handle_out) noexcept;

/// Get the remote audio track associated with this transceiver, if any.
/// The returned handle holds a reference to the video transceiver, which must
/// be released with |mrsRemoteAudioTrackRemoveRef()| once not needed anymore.
MRS_API mrsResult MRS_CALL mrsAudioTransceiverGetRemoteTrack(
    AudioTransceiverHandle transceiver_handle,
    RemoteAudioTrackHandle* track_handle_out) noexcept;

}  // extern "C"

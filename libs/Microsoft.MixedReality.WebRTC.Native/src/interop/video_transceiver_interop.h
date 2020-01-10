// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "export.h"
#include "interop/interop_api.h"

extern "C" {

/// Add a reference to the native object associated with the given handle.
MRS_API void MRS_CALL
mrsVideoTransceiverAddRef(VideoTransceiverHandle handle) noexcept;

/// Remove a reference from the native object associated with the given handle.
MRS_API void MRS_CALL
mrsVideoTransceiverRemoveRef(VideoTransceiverHandle handle) noexcept;

/// Get the local video track associated with this transceiver, if any.
MRS_API mrsResult MRS_CALL mrsVideoTransceiverGetLocalTrack(
    VideoTransceiverHandle transceiver_handle,
    LocalVideoTrackHandle* track_handle_out) noexcept;

/// Get the remote video track associated with this transceiver, if any.
MRS_API mrsResult MRS_CALL mrsVideoTransceiverGetRemoteTrack(
    VideoTransceiverHandle transceiver_handle,
    RemoteVideoTrackHandle* track_handle_out) noexcept;

}  // extern "C"

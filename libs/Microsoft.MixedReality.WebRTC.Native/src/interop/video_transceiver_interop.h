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

/// Set the local video track associated with this transceiver. This new track
/// replaces the existing one, if any. This doesn't require any SDP
/// renegotiation.
MRS_API mrsResult MRS_CALL
mrsVideoTransceiverSetLocalTrack(VideoTransceiverHandle transceiver_handle,
                                 LocalVideoTrackHandle track_handle) noexcept;

/// Get the local video track associated with this transceiver, if any.
/// The returned handle holds a reference to the video transceiver, which must
/// be released with |mrsLocalVideoTrackRemoveRef()| once not needed anymore.
MRS_API mrsResult MRS_CALL mrsVideoTransceiverGetLocalTrack(
    VideoTransceiverHandle transceiver_handle,
    LocalVideoTrackHandle* track_handle_out) noexcept;

/// Get the remote video track associated with this transceiver, if any.
/// The returned handle holds a reference to the video transceiver, which must
/// be released with |mrsRemoteVideoTrackRemoveRef()| once not needed anymore.
MRS_API mrsResult MRS_CALL mrsVideoTransceiverGetRemoteTrack(
    VideoTransceiverHandle transceiver_handle,
    RemoteVideoTrackHandle* track_handle_out) noexcept;

}  // extern "C"
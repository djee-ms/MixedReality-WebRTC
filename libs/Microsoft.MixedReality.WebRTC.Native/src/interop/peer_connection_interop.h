// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "export.h"
#include "interop/interop_api.h"

extern "C" {

//
// Wrapper
//

/// Add a reference to the native object associated with the given handle.
MRS_API void MRS_CALL
mrsPeerConnectionAddRef(PeerConnectionHandle handle) noexcept;

/// Remove a reference from the native object associated with the given handle.
MRS_API void MRS_CALL
mrsPeerConnectionRemoveRef(PeerConnectionHandle handle) noexcept;

/// Callback fired when the state of the ICE connection changed.
using mrsPeerConnectionIceGatheringStateChangedCallback =
    void(MRS_CALL*)(void* user_data, IceGatheringState new_state);

/// Register a callback fired when the ICE connection state changes.
MRS_API void MRS_CALL mrsPeerConnectionRegisterIceGatheringStateChangedCallback(
    PeerConnectionHandle peerHandle,
    mrsPeerConnectionIceGatheringStateChangedCallback callback,
    void* user_data) noexcept;

MRS_API mrsResult MRS_CALL mrsPeerConnectionAddAudioTransceiver(
    PeerConnectionHandle peer_handle,
    const char* name,
    const AudioTransceiverConfiguration* config,
    AudioTransceiverHandle* handle) noexcept;

MRS_API mrsResult MRS_CALL mrsPeerConnectionAddVideoTransceiver(
    PeerConnectionHandle peer_handle,
    const char* name,
    const VideoTransceiverConfiguration* config,
    VideoTransceiverHandle* handle) noexcept;

}  // extern "C"

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "../include/mrs_errors.h"
#include "../src/interop/global_factory.h"
#include "../src/interop/interop_api.h"
#include "../src/interop/peer_connection_interop.h"

using namespace Microsoft::MixedReality::WebRTC;

namespace VideoTestUtils {

mrsResult MRS_CALL MakeTestFrame(void* /*user_data*/,
                                 ExternalVideoTrackSourceHandle handle,
                                 uint32_t request_id,
                                 int64_t timestamp_ms);

void CheckIsTestFrame(const I420AVideoFrame& frame);

}

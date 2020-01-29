// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <cassert>

#include <condition_variable>
#include <functional>
#include <mutex>

using namespace std::chrono_literals;

#if defined(MR_SHARING_WIN)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

//#include <windows.h>
#include <SDKDDKVer.h>

#else defined(MR_SHARING_ANDROID)

#endif

// Workaround https://github.com/google/googletest/issues/1111
#define GTEST_LANG_CXX11 1

// Google Test
#include "gtest/gtest.h"

// Microsoft.MixedReality.WebRTC (C++ Library)
#include "Microsoft.MixedReality.WebRTC.h"

// Local test helpers
#include "peer_connection_test_helpers.h"

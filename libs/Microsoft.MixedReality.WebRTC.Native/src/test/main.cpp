// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

// Force a main() function because webrtc.lib seems to import many of them from
// various tools, probably due to misconfigured gn files and/or default extern
// visibility of global functions on Windows.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

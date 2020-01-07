# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10.2)

# WebRTC core repository location, where the native headers are located.
# This must be set by the user to the location of the repository they cloned.
if (NOT WEBRTC_REPO)
    message(FATAL_ERROR "The WEBRTC_REPO variable needs to be set to the location of the Google repository")
endif()
get_filename_component(WEBRTC_REPO ${WEBRTC_REPO} ABSOLUTE)
message("Using local WebRTC repository at ${WEBRTC_REPO}")

# Path to the deps/ folder where the libwebrtc.a is copied
set(DEPS_DIR ${CMAKE_SOURCE_DIR}/../../../../deps)

# Path to the MixedReality-WebRTC native C++ library root folder
set(MR_WEBRTC_NATIVE_DIR ../../../../..)

add_library(Microsoft.MixedReality.WebRTC.Native SHARED
    ${MR_WEBRTC_NATIVE_DIR}/src/interop/external_video_track_source_interop.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/interop/global_factory.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/interop/interop_api.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/interop/local_video_track_interop.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/interop/peer_connection_interop.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/media/external_video_track_source.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/media/local_video_track.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/audio_frame_observer.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/data_channel.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/mrs_errors.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/pch.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/peer_connection.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/sdp_utils.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/str.cpp
    ${MR_WEBRTC_NATIVE_DIR}/src/video_frame_observer.cpp
    ./jni_onload.cpp
)

# Import the libwebrtc.a library as an externally-built target
add_library(libwebrtc STATIC IMPORTED)
set_target_properties(libwebrtc PROPERTIES IMPORTED_LOCATION
    ${DEPS_DIR}/webrtc/${CMAKE_ANDROID_ARCH_ABI}/libwebrtc.a
)

# Configure the C++ include paths
target_include_directories(Microsoft.MixedReality.WebRTC.Native PRIVATE
    ${MR_WEBRTC_NATIVE_DIR}/include
    ${MR_WEBRTC_NATIVE_DIR}/src
    ${WEBRTC_REPO}
    ${WEBRTC_REPO}/third_party/abseil-cpp
    ${WEBRTC_REPO}/third_party/libyuv/include
)

# Create the Microsoft.MixedReality.WebRTC.Native.so target
target_link_libraries(Microsoft.MixedReality.WebRTC.Native
    log
    android
    dl
    OpenSLES
    libwebrtc
)

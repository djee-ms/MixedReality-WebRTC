
cmake_minimum_required(VERSION 3.4.1)

project(mr-webrtc)

add_library(Microsoft.MixedReality.WebRTC.Native SHARED
    ../../../libs/Microsoft.MixedReality.WebRTC.Native/src/interop/interop_api.cpp
    ../../../libs/Microsoft.MixedReality.WebRTC.Native/src/audio_frame_observer.cpp
    ../../../libs/Microsoft.MixedReality.WebRTC.Native/src/data_channel.cpp
    ../../../libs/Microsoft.MixedReality.WebRTC.Native/src/peer_connection.cpp
    ../../../libs/Microsoft.MixedReality.WebRTC.Native/src/sdp_utils.cpp
    ../../../libs/Microsoft.MixedReality.WebRTC.Native/src/str.cpp
    ../../../libs/Microsoft.MixedReality.WebRTC.Native/src/video_frame_observer.cpp
)

target_include_directories(Microsoft.MixedReality.WebRTC.Native
    PUBLIC
    ../../../libs/Microsoft.MixedReality.WebRTC.Native/include
    PRIVATE
    ../../../libs/Microsoft.MixedReality.WebRTC.Native/src
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.17763.0/shared"
    "../../../external/webrtc-uwp-sdk/webrtc/xplatform/webrtc"
    "../../../external/webrtc-uwp-sdk/webrtc/xplatform/webrtc/third_party/libyuv/include"
    "../../../external/webrtc-uwp-sdk/webrtc/xplatform/webrtc/third_party/abseil-cpp"
)

add_definitions(
    -DMR_SHARING_ANDROID
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++17")

# TODO - FIXME
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")

# Find the Android log library for native platform logging (logcat)
find_library(LIB_ANDROID_LOG log)

# Find the core WebRTC implementation (libwebrtc.o)
find_library(LIB_WEBRTC webrtc PATHS ../../../external/webrtc-uwp-sdk/webrtc/xplatform/webrtc/out/webrtc_android_x64_release/obj)

target_link_libraries(Microsoft.MixedReality.WebRTC.Native
    ${LIB_ANDROID_LOG}
    ${LIB_WEBRTC}
)

#/bin/sh

# Copy per-architecture libwebrtc.a variants to deps/webrtc/ folder
mkdir -p ../../libs/Microsoft.MixedReality.WebRTC.Native/android/deps/armeabi-v7a/
mkdir -p ../../libs/Microsoft.MixedReality.WebRTC.Native/android/deps/arm64-v8a/
mkdir -p ../../libs/Microsoft.MixedReality.WebRTC.Native/android/deps/x86/
cp ../../../webrtc_android/src/out/android_arm32/obj/libwebrtc.a ../../libs/Microsoft.MixedReality.WebRTC.Native/android/deps/webrtc/armeabi-v7a/
cp ../../../webrtc_android/src/out/android_arm64/obj/libwebrtc.a ../../libs/Microsoft.MixedReality.WebRTC.Native/android/deps/webrtc/arm64-v8a/
cp ../../../webrtc_android/src/out/android_x86/obj/libwebrtc.a ../../libs/Microsoft.MixedReality.WebRTC.Native/android/deps/webrtc/x86/

# Copy Java wrapper to deps/webrtc/ folder
cp ../../../webrtc_android/src/out/android_arm64/lib.java/sdk/android/libwebrtc.jar ../../libs/Microsoft.MixedReality.WebRTC.Native/android/deps/webrtc/

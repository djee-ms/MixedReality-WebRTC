//
//  video_capture_ios.m
//  Microsoft.MixedReality.WebRTC
//
//  Created by Jérôme HUMBERT on 14/06/2020.
//  Copyright © 2020 Microsoft. All rights reserved.
//

#include "pch.h"

#import <Foundation/Foundation.h>

#import <WebRTC/RTCVideoTrack.h>
#import <WebRTC/RTCCameraVideoCapturer.h>

#include "rtc_base/ref_counted_object.h"
#include "sdk/objc/native/api/video_capturer.h"

#include "video_capture_ios.h"

//namespace Microsoft {
//  namespace MixedReality {
//    namespace WebRTC {
//    }
//  }
//}

using namespace Microsoft::MixedReality::WebRTC;

rtc::scoped_refptr<MRWebRTCVideoCaptureIOS> MRWebRTCVideoCaptureIOS::Create(
  const RefPtr<GlobalFactory>& global_factory)
{
  rtc::scoped_refptr<MRWebRTCVideoCaptureIOS> source =
    new rtc::RefCountedObject<MRWebRTCVideoCaptureIOS>();
  if (!source) {
    return {};
  }

  const mrsResult res = source->init(*global_factory);
  if (res != mrsResult::kSuccess) {
    return {};
  }
  return source;
}

MRWebRTCVideoCaptureIOS::MRWebRTCVideoCaptureIOS()
{
}

mrsResult MRWebRTCVideoCaptureIOS::init(GlobalFactory& global_factory)
{
  // Create the Objective-C interop layer
  auto capturer = [[RTCCameraVideoCapturer alloc] init];
  if (!capturer) {
    return mrsResult::kUnknownError;
  }
  
  // Check user authorization
  __block bool authorized = false;
  switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo]) {
    case AVAuthorizationStatusAuthorized:
      authorized = true;
      break;
    case AVAuthorizationStatusNotDetermined:
    	{
        RTC_CHECK(![NSThread isMainThread]);
        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
          authorized = granted;
          dispatch_semaphore_signal(sema);
        }];
        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
      }
      break;
    case AVAuthorizationStatusDenied:
      RTC_LOG(LS_ERROR) << "User denied access to video capture.";
      return mrsResult::kUnsupported;
    case AVAuthorizationStatusRestricted:
      RTC_LOG(LS_ERROR) << "Cannot authorize video capture due to restriction.";
      return mrsResult::kUnsupported;
  }
  
  // Get the capture device
  NSArray<AVCaptureDevice *> *captureDevices = [RTCCameraVideoCapturer captureDevices];
  for (AVCaptureDevice *device in captureDevices) {
    const char* const deviceId = [device.uniqueID UTF8String];
    RTC_LOG(LS_INFO) << "Found video capture device " << deviceId;
  }
  AVCaptureDevice *device = captureDevices[0];
  
  // Get the capture format
  NSArray<AVCaptureDeviceFormat *> *formats = [RTCCameraVideoCapturer supportedFormatsForDevice:device];
  for (AVCaptureDeviceFormat *format in formats) {
    CMVideoDimensions dimension = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
    FourCharCode pixelFormat = CMFormatDescriptionGetMediaSubType(format.formatDescription);
    RTC_LOG(LS_INFO) << "Found video capture format " << dimension.width << "x" << dimension.height << " (fourcc=" << pixelFormat << ")";
    for (AVFrameRateRange *fpsRange in format.videoSupportedFrameRateRanges) {
      RTC_LOG(LS_INFO) << "+ fps = " << fpsRange.maxFrameRate;
    }
  }
  
  source_ = webrtc::ObjCToNativeVideoCapturer(capturer,
                                           global_factory.GetSignalingThread(),
                                           global_factory.GetWorkerThread());
  if (!source_) {
    return mrsResult::kUnknownError;
  }
  return mrsResult::kSuccess;
}

//@interface MRWebRTCVideoCaptureImpl : NSObject
//
//- (instancetype)initWithSource;
//
//@end
//
//
//@implementation MRWebRTCVideoCaptureImpl
//
//- (instancetype)initWithSource {
//  return [MRWebRTCVideoCapture alloc];
//}
//
//- (RTCVideoTrack *)createLocalVideoTrack {
////  if ([_settings currentAudioOnlySettingFromStore]) {
////    return nil;
////  }
//
//  RTCVideoSource *source = [_factory videoSource];
//
//  RTCCameraVideoCapturer *capturer = [[RTCCameraVideoCapturer alloc] initWithDelegate:source];
//  //[_delegate appClient:self didCreateLocalCapturer:capturer];
//
//  return [_factory videoTrackWithSource:source trackId:kARDVideoTrackId];
//}
//
//@end

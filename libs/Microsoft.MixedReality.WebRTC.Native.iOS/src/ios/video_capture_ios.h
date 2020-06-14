//
//  video_capture_ios.h
//  Microsoft.MixedReality.WebRTC
//
//  Created by Jérôme HUMBERT on 14/06/2020.
//  Copyright © 2020 Microsoft. All rights reserved.
//

#pragma once

#if !defined(MR_SHARING_IOS)
#error This file should only be used on iOS
#endif

//#include "ref_counted_base.h"
//#include "refptr.h"
#include "result.h"
#include "global_factory.h"

#include "api/media_stream_interface.h"

using namespace Microsoft::MixedReality::WebRTC;

class MRWebRTCVideoCaptureIOS : public webrtc::VideoTrackSourceInterface
{
public:
  static rtc::scoped_refptr<MRWebRTCVideoCaptureIOS> Create(
    const std::unique_ptr<GlobalFactory>& global_factory);
  
  //
  // NotifierInterface
  //
  
  void RegisterObserver(webrtc::ObserverInterface* observer) override {}
  void UnregisterObserver(webrtc::ObserverInterface* observer) override {}
  
  
  //
  // MediaSourceInterface
  //
  
  SourceState state() const override { return SourceState::kLive; }
  
  bool remote() const override { return false; }
  
  
  //
  // VideoSourceInterface<VideoFrame>
  //
  
  
  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                               const rtc::VideoSinkWants& wants) override {
    sinks_.push_back(sink);
  }
  
  // RemoveSink must guarantee that at the time the method returns,
  // there is no current and no future calls to VideoSinkInterface::OnFrame.
  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
    auto it = std::find(sinks_.begin(), sinks_.end(), sink);
    if (it != sinks_.end()) {
      sinks_.erase(it);
    }
  }

  
  //
  // VideoTrackSourceInterface
  //
  
  // Indicates that parameters suitable for screencasts should be automatically
  // applied to RtpSenders.
  bool is_screencast() const override { return false; }
  
  // Indicates that the encoder should denoise video before encoding it.
  // If it is not set, the default configuration is used which is different
  // depending on video codec.
  absl::optional<bool> needs_denoising() const override { return false; }
  
  // Returns false if no stats are available, e.g, for a remote source, or a
  // source which has not seen its first frame yet.
  //
  // Implementation should avoid blocking.
  bool GetStats(Stats* stats) override { return false; }
  
  // Add an encoded video sink to the source and additionally cause
  // a key frame to be generated from the source. The sink will be
  // invoked from a decoder queue.
  void AddEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override {
    encoded_sinks_.push_back(sink);
  }
  
  // Removes an encoded video sink from the source.
  void RemoveEncodedSink(rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override {
    auto it = std::find(encoded_sinks_.begin(), encoded_sinks_.end(), sink);
    if (it != encoded_sinks_.end()) {
      encoded_sinks_.erase(it);
    }
  }
  
//private:
  MRWebRTCVideoCaptureIOS();
  mrsResult init(GlobalFactory& global_factory);
  
private:
  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source_;
  std::vector<rtc::VideoSinkInterface<webrtc::VideoFrame>*> sinks_;
  std::vector<rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>*> encoded_sinks_;
};

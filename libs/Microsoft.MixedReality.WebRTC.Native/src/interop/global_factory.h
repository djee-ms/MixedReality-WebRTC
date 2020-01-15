// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "export.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

enum class ObjectType : int {
  kPeerConnection,
  kLocalAudioTrack,
  kLocalVideoTrack,
  kExternalVideoTrackSource,
  kRemoteAudioTrack,
  kRemoteVideoTrack,
  kDataChannel,
  kAudioTransceiver,
  kVideoTransceiver,
};

/// Global factory wrapper adding thread safety to all global objects, including
/// the peer connection factory, and on UWP the so-called "WebRTC factory".
class GlobalFactory {
 public:
  /// Set the MixedReality-WebRTC library shutdown options.
  static void SetShutdownOptions(mrsShutdownOptions options) noexcept;

  /// Global factory of all global objects, including the peer connection
  /// factory itself, with added thread safety.
  static RefPtr<GlobalFactory> InstancePtr();

  /// Attempt to shutdown the global factory if no live object is present
  /// anymore. This is always conservative and safe, and will do nothing if any
  /// object is still live.
  static bool TryShutdown() noexcept;

  GlobalFactory() = default;
  ~GlobalFactory();

  /// Get the existing peer connection factory, or NULL if not created.
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
  GetPeerConnectionFactory() noexcept;

  /// Get the worker thread. This is only valid if initialized.
  rtc::Thread* GetWorkerThread() noexcept;

  /// Add to the global factory collection an object whose lifetime must be
  /// tracked to know when it is safe to terminate the WebRTC threads. This is
  /// generally called form the object's constructor for safety.
  void AddObject(ObjectType type, TrackedObject* obj) noexcept;

  /// Remove an object added with |AddObject|. This is generally called from the
  /// object's destructor for safety.
  void RemoveObject(ObjectType type, TrackedObject* obj) noexcept;

  /// Report live objects to WebRTC logging system for debugging.
  /// This is automatically called if the |mrsShutdownOptions::kLogLiveObjects|
  /// shutdown option is set, but can also be called manually at any time.
  void ReportLiveObjects();

#if defined(WINUWP)
  using WebRtcFactoryPtr =
      std::shared_ptr<wrapper::impl::org::webRtc::WebRtcFactory>;
  WebRtcFactoryPtr get();
  mrsResult GetOrCreateWebRtcFactory(WebRtcFactoryPtr& factory);
#endif  // defined(WINUWP)

  void AddRef() const noexcept {
    temp_ref_count_.fetch_add(1, std::memory_order_relaxed);
  }

  void RemoveRef() const noexcept {
    if (temp_ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      GlobalFactory::TryShutdown();
    }
  }

 private:
  GlobalFactory(const GlobalFactory&) = delete;
  GlobalFactory& operator=(const GlobalFactory&) = delete;
  static std::unique_ptr<GlobalFactory>& MutableInstance();
  mrsResult InitializeImpl();
  void ForceShutdownImpl();
  bool TryShutdownImpl();
  void ReportLiveObjectsNoLock();

 private:
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_
      RTC_GUARDED_BY(mutex_);
#if defined(WINUWP)
  WebRtcFactoryPtr impl_ RTC_GUARDED_BY(mutex_);
#else   // defined(WINUWP)
  std::unique_ptr<rtc::Thread> network_thread_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<rtc::Thread> worker_thread_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<rtc::Thread> signaling_thread_ RTC_GUARDED_BY(mutex_);
#endif  // defined(WINUWP)
  std::recursive_mutex mutex_;

  /// Collection of all objects alive.
  std::unordered_map<TrackedObject*, ObjectType> alive_objects_
      RTC_GUARDED_BY(mutex_);

  /// Shutdown options.
  mrsShutdownOptions shutdown_options_;

  /// Reference count for RAII-style shutdown. This is not used as a true
  /// reference count, but instead as a marker of temporary acquiring a
  /// reference to the GlobalFactory while trying to create some objects, and as
  /// a notification mechanism when said reference is released to check for
  /// shutdown. Therefore most of the time the reference count is zero, yet the
  /// instance stays alive.
  mutable std::atomic_uint32_t temp_ref_count_{0};
};

}  // namespace Microsoft::MixedReality::WebRTC

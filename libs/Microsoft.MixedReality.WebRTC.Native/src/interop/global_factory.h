// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "export.h"
#include "peer_connection.h"
#include "utils.h"

#include <shared_mutex>

namespace Microsoft::MixedReality::WebRTC {

/// Global factory wrapper adding thread safety to all global objects, including
/// the peer connection factory, and on UWP the so-called "WebRTC factory".
class GlobalFactory {
 public:
  /// Report live objects to debug output, and return the number of live objects
  /// at the time of the call. If the library is not initialized, this function
  /// returns 0. This is multithread-safe.
  static uint32_t StaticReportLiveObjects() noexcept;

  /// Set the library shutdown options. This function does not initialize the
  /// library, but will store the options for a future initializing. Conversely,
  /// if the library is already initialized then the options are set
  /// immediately. This is multithread-safe.
  static void SetShutdownOptions(mrsShutdownOptions options) noexcept;

  /// Force-shutdown the library if it is initialized, or does nothing
  /// otherwise. This call will terminate the WebRTC threads, therefore will
  /// prevent any dispatched call to a WebRTC object from completing. However,
  /// by shutting down the threads it will allow unloading the current module
  /// (DLL), so is recommended to call manually at the end of the process when
  /// WebRTC objects are not in use anymore but before static deinitializing.
  /// This is multithread-safe.
  static void ForceShutdown() noexcept;

  /// Attempt to shutdown the library if no live object is present anymore. This
  /// is always conservative and safe, and will do nothing if any object is
  /// still alive. The function returns |true| if the library is shut down after
  /// the call, either because it was already or because the call did shut it
  /// down. This is multithread-safe.
  static bool TryShutdown() noexcept;

  /// Try to acquire a shared lock to the global factory singleton instance, and
  /// return a locking pointer holding that lock. If the library is not
  /// initialized, this returns a NULL pointer. This is multithread-safe.
  static RefPtr<GlobalFactory> GetLockIfExist() {
    return GetLockImpl(/* ensureInitialized = */ false);
  }

  /// Acquire a shared lock to the global factory singleton instance, and return
  /// a locking pointer holding that lock. If the library is not initialized,
  /// this call initializes it. This is multithread-safe.
  static RefPtr<GlobalFactory> GetLock() {
    return GetLockImpl(/* ensureInitialized = */ true);
  }

  void AddRef() const noexcept {
    // Calling the member function |AddRef()| implies already holding a
    // reference, expect for |GetLockImpl()| which will acquire the init mutex
    // so cannot run concurrently with |RemoveRef()|.
    RTC_DCHECK(factory_);
    ref_count_.fetch_add(1, std::memory_order_relaxed);
  }

  void RemoveRef() const noexcept {
    // Update the reference count under the init lock to ensure it cannot change
    // from another thread, since invoking |AddRef()| requires having already a
    // pointer to the GlobalFactory (so this reference would never be the last
    // one) or calling |GetLockImpl()| to get a new pointer, which will only
    // call |AddRef()| under the lock too so will block.
    std::scoped_lock lock(init_mutex_);
    RTC_DCHECK(factory_);
    if (ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      const_cast<GlobalFactory*>(this)->ShutdownImplNoLock(
          ShutdownAction::kTryShutdownIfSafe);
    }
  }

  /// Get the existing peer connection factory, or NULL if the library is not
  /// initialized.
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
  GetPeerConnectionFactory() noexcept;

  /// Get the WebRTC background worker thread, or NULL if the library is not
  /// initialized.
  rtc::Thread* GetWorkerThread() const noexcept;

  /// Add to the global factory collection an object whose lifetime must be
  /// tracked to know when it is safe to shutdown the library and terminate the
  /// WebRTC threads. This is generally called form a wrapper object's
  /// constructor for safety.
  void AddObject(TrackedObject* obj) noexcept;

  /// Remove an object added with |AddObject|. This is generally called from a
  /// wrapper object's destructor for safety.
  void RemoveObject(TrackedObject* obj) noexcept;

  /// Report live objects to WebRTC logging system for debugging.
  /// This is automatically called if the |mrsShutdownOptions::kLogLiveObjects|
  /// shutdown option is set, but can also be called manually at any time.
  /// Return the number of live objects at the time of the call, which can be
  /// outdated as soon as the call returns if other threads add/remove objects.
  uint32_t ReportLiveObjects();

#if defined(WINUWP)
  using WebRtcFactoryPtr =
      std::shared_ptr<wrapper::impl::org::webRtc::WebRtcFactory>;
  WebRtcFactoryPtr get();
  mrsResult GetOrCreateWebRtcFactory(WebRtcFactoryPtr& factory);
#endif  // defined(WINUWP)

 private:
  friend struct std::default_delete<GlobalFactory>;
  static GlobalFactory* GetInstance();
  static RefPtr<GlobalFactory> GetLockImpl(bool ensureInitialized);
  GlobalFactory() = default;
  ~GlobalFactory();
  GlobalFactory(const GlobalFactory&) = delete;
  GlobalFactory& operator=(const GlobalFactory&) = delete;
  bool IsInitialized() const noexcept {
    std::scoped_lock lock(init_mutex_);
    return factory_ != nullptr;
  }
  mrsResult InitializeImplNoLock();
  enum class ShutdownAction {
    /// Try to safely shutdown, only if no object is still alive.
    kTryShutdownIfSafe,
    /// Force shutdown even if some objects are still alive.
    kForceShutdown,
    /// Shutting down from ~GlobalFactory(), ignoring any bypass and try to
    /// destroy everything.
    kFromObjectDestructor
  };
  /// Shutdown the library. Return |true| if the library was shut down, or
  /// |false| otherwise.
  bool ShutdownImplNoLock(ShutdownAction shutdown_action);
  void ReportLiveObjectsNoLock();

 private:
  /// Shared mutex for factory initializing and shutdown. This is acquired
  /// read-only when retrieving a RefPtr<> to the singleton instance, and will
  /// therefore prevent destruction during this time. For init and shutdown
  /// operations, this requires exclusive write access.
  mutable std::mutex init_mutex_;

  /// Global peer connection factory. This is initialized only while the library
  /// is initialized, and is immutable between init and shutdown, so do not
  /// require |mutex_| for access.
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_
      RTC_GUARDED_BY(init_mutex_);

#if defined(WINUWP)

  /// UWP factory for WinRT wrapper layer. This is initialized only while the
  /// library is initialized, and is immutable between init and shutdown, so do
  /// not require |mutex_| for access.
  WebRtcFactoryPtr impl_ RTC_GUARDED_BY(init_mutex_);

#else  // defined(WINUWP)

  /// WebRTC networking thread. This is initialized only while the library
  /// is initialized, and is immutable between init and shutdown, so do not
  /// require |mutex_| for access.
  std::unique_ptr<rtc::Thread> network_thread_ RTC_GUARDED_BY(init_mutex_);

  /// WebRTC background worker thread. This is initialized only while the
  /// library is initialized, and is immutable between init and shutdown, so do
  /// not require |mutex_| for access.
  std::unique_ptr<rtc::Thread> worker_thread_ RTC_GUARDED_BY(init_mutex_);

  /// WebRTC signaling thread. This is initialized only while the library
  /// is initialized, and is immutable between init and shutdown, so do not
  /// require |mutex_| for access.
  std::unique_ptr<rtc::Thread> signaling_thread_ RTC_GUARDED_BY(init_mutex_);

#endif  // defined(WINUWP)

  mutable std::atomic_uint32_t ref_count_{0};

  /// Recursive mutex for thread-safety of calls to this instance.
  /// This is used to protect members not related with init/shutdown, while the
  /// caller holds a read-only shared access to |init_mutex_|, and is recursive
  /// to simplify using those.
  /// Note that all objects protected by this lock are automatically protected
  /// by an exclusive lock on |init_mutex_|, since an external user first need
  /// to acquire read-only shared access to |init_mutex_| before being able to
  /// get a reference to the singleton instance to call its member methods.
  mutable std::recursive_mutex mutex_;

  /// Collection of all objects alive.
  std::unordered_map<TrackedObject*, ObjectType> alive_objects_
      RTC_GUARDED_BY(mutex_);

  /// Shutdown options.
  mrsShutdownOptions shutdown_options_ RTC_GUARDED_BY(mutex_) =
      mrsShutdownOptions::kDefault;
};

}  // namespace Microsoft::MixedReality::WebRTC

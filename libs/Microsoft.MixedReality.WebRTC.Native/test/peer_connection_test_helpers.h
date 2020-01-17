// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "../include/mrs_errors.h"
#include "../src/interop/interop_api.h"
#include "../src/interop/peer_connection_interop.h"

using namespace Microsoft::MixedReality::WebRTC;

/// Simple wait event, similar to rtc::Event.
struct Event {
  void Reset() {
    std::unique_lock<std::mutex> lk(m_);
    signaled_ = false;
  }
  void Set() {
    std::unique_lock<std::mutex> lk(m_);
    signaled_ = true;
    cv_.notify_one();
  }
  void SetBroadcast() {
    std::unique_lock<std::mutex> lk(m_);
    signaled_ = true;
    cv_.notify_all();
  }
  bool IsSignaled() const {
    std::unique_lock<std::mutex> lk(m_);
    return signaled_;
  }
  void Wait() {
    std::unique_lock<std::mutex> lk(m_);
    if (!signaled_) {
      cv_.wait(lk);
    }
  }
  bool WaitFor(std::chrono::seconds seconds) {
    std::unique_lock<std::mutex> lk(m_);
    if (!signaled_) {
      return (cv_.wait_for(lk, seconds) == std::cv_status::no_timeout);
    }
    return true;
  }
  mutable std::mutex m_;
  std::condition_variable cv_;
  bool signaled_{false};
};

/// Simple semaphore.
struct Semaphore {
  void Acquire(int64_t count = 1) {
    std::unique_lock<std::mutex> lk(m_);
    while (value_ < count) {
      cv_.wait(lk);
    }
    value_ -= count;
  }
  bool TryAcquireFor(std::chrono::seconds seconds, int64_t count = 1) {
    std::unique_lock<std::mutex> lk(m_);
    while (value_ < count) {
      if (cv_.wait_for(lk, seconds) == std::cv_status::timeout) {
        return false;
      }
    }
    value_ -= count;
    return true;
  }
  void Release(int64_t count = 1) {
    std::unique_lock<std::mutex> lk(m_);
    const int64_t old_value = value_;
    value_ += count;
    if (old_value <= 0) {
      cv_.notify_all();
    }
  }
  std::mutex m_;
  std::condition_variable cv_;
  int64_t value_{0};
};

/// RAII helper to initialize and shutdown the library.
struct LibraryInitRaii {
  // TODO - remove this
};

/// Wrapper around an interop callback taking an extra raw pointer argument, to
/// trampoline its call to a generic std::function for convenience (including
/// lambda functions).
/// Use the CB() macro to register the callback with the interop layer.
/// Note that the InteropCallback<> variable must stay in scope for the duration
/// of the use, so that its static callback function is kept alive while
/// registered.
/// Usage:
///   {
///     InteropCallback<int> cb([](int i) { ... });
///     mrsRegisterXxxCallback(h, CB(cb));
///     [...]
///     mrsRegisterXxxCallback(h, nullptr, nullptr);
///   }
template <typename... Args>
struct InteropCallback {
  /// Callback function signature type.
  using function_type = void(Args...);

  /// Type of the static interop callback, which prepends a void* user data
  /// opaque argument.
  using static_type = void(MRS_CALL*)(void*, Args...);

  InteropCallback() = default;
  virtual ~InteropCallback() { assert(!is_registered_); }

  /// Constructor from any std::function-compatible object, including lambdas.
  template <typename U>
  InteropCallback(U func)
      : func_(std::function<function_type>(std::forward<U>(func))) {}

  template <typename U>
  InteropCallback& operator=(U func) {
    func_ = std::function<function_type>(std::forward<U>(func));
    return (*this);
  }

  /// Adapter for generic std::function<> to interop callback.
  static void MRS_CALL StaticExec(void* user_data, Args... args) {
    auto self = (InteropCallback*)user_data;
    self->func_(std::forward<Args>(args)...);
  }

  std::function<function_type> func_;
  bool is_registered_{false};
};

/// Convenience macro to fill the interop callback registration arguments.
#define CB(x) &x.StaticExec, &x

/// Helper to create and close a peer connection.
class PCRaii {
 public:
  /// Create a peer connection without any default ICE server.
  /// Generally tests use direct hard-coded SDP message passing,
  /// so do not need NAT traversal nor even local networking.
  /// Note that due to a limitation/bug in the implementation, complete lack of
  /// networking (e.g. airplane mode, or no network interface) will prevent the
  /// connection from being established.
  PCRaii() {
    PeerConnectionConfiguration config{};
    mrsPeerConnectionInteropHandle interop_handle = (void*)0x1;
    create(config, interop_handle);
  }
  /// Create a peer connection with a specific configuration.
  PCRaii(const PeerConnectionConfiguration& config,
         mrsPeerConnectionInteropHandle interop_handle = (void*)0x1) {
    create(config, interop_handle);
  }
  ~PCRaii() { mrsPeerConnectionRemoveRef(handle_); }
  PeerConnectionHandle handle() const { return handle_; }

 protected:
  PeerConnectionHandle handle_{};
  void create(const PeerConnectionConfiguration& config,
              mrsPeerConnectionInteropHandle interop_handle) {
    ASSERT_EQ(mrsResult::kSuccess,
              mrsPeerConnectionCreate(config, interop_handle, &handle_));
    ASSERT_NE(nullptr, handle_);
  }
};

// OnLocalSdpReadyToSend
class SdpCallback : public InteropCallback<const char*, const char*> {
 public:
  using Base = InteropCallback<const char*, const char*>;
  using callback_type = void(const char*, const char*);
  SdpCallback(PeerConnectionHandle pc) : pc_(pc) {}
  SdpCallback(PeerConnectionHandle pc, std::function<callback_type> func)
      : Base(std::move(func)), pc_(pc) {
    mrsPeerConnectionRegisterLocalSdpReadytoSendCallback(pc_, &StaticExec,
                                                         this);
    is_registered_ = true;
  }
  ~SdpCallback() override {
    mrsPeerConnectionRegisterLocalSdpReadytoSendCallback(pc_, nullptr, nullptr);
    is_registered_ = false;
  }
  SdpCallback& operator=(std::function<callback_type> func) {
    Base::operator=(std::move(func));
    mrsPeerConnectionRegisterLocalSdpReadytoSendCallback(pc_, &StaticExec,
                                                         this);
    is_registered_ = true;
    return (*this);
  }

 protected:
  PeerConnectionHandle pc_{};
};

// OnIceCandidateReadyToSend
class IceCallback : public InteropCallback<const char*, int, const char*> {
 public:
  using Base = InteropCallback<const char*, int, const char*>;
  using callback_type = void(const char*, int, const char*);
  IceCallback(PeerConnectionHandle pc) : pc_(pc) {}
  IceCallback(PeerConnectionHandle pc, std::function<callback_type> func)
      : Base(std::move(func)), pc_(pc) {
    mrsPeerConnectionRegisterIceCandidateReadytoSendCallback(pc_, &StaticExec,
                                                             this);
    is_registered_ = true;
  }
  ~IceCallback() override {
    mrsPeerConnectionRegisterIceCandidateReadytoSendCallback(pc_, nullptr,
                                                             nullptr);
    is_registered_ = false;
  }
  IceCallback& operator=(std::function<callback_type> func) {
    Base::operator=(std::move(func));
    mrsPeerConnectionRegisterIceCandidateReadytoSendCallback(pc_, &StaticExec,
                                                             this);
    is_registered_ = true;
    return (*this);
  }

 protected:
  PeerConnectionHandle pc_{};
};

constexpr const std::string_view kOfferString{"offer"};

/// Helper to create a pair of peer connections and locally connect them to each
/// other via simple hard-coded signaling.
class LocalPeerPairRaii {
 public:
  LocalPeerPairRaii()
      : sdp1_cb_(pc1()), sdp2_cb_(pc2()), ice1_cb_(pc1()), ice2_cb_(pc2()) {
    setup();
  }
  LocalPeerPairRaii(const PeerConnectionConfiguration& config)
      : pc1_(config),
        pc2_(config),
        sdp1_cb_(pc1()),
        sdp2_cb_(pc2()),
        ice1_cb_(pc1()),
        ice2_cb_(pc2()) {
    setup();
  }
  LocalPeerPairRaii(const PeerConnectionConfiguration& config,
                    mrsPeerConnectionInteropHandle h1,
                    mrsPeerConnectionInteropHandle h2)
      : pc1_(config, h1),
        pc2_(config, h2),
        sdp1_cb_(pc1()),
        sdp2_cb_(pc2()),
        ice1_cb_(pc1()),
        ice2_cb_(pc2()) {
    setup();
  }
  ~LocalPeerPairRaii() { shutdown(); }

  PeerConnectionHandle pc1() const { return pc1_.handle(); }
  PeerConnectionHandle pc2() const { return pc2_.handle(); }

  void ConnectAndWait() {
    Event ev1, ev2;
    connected1_cb_ = [&ev1]() { ev1.Set(); };
    connected2_cb_ = [&ev2]() { ev2.Set(); };
    mrsPeerConnectionRegisterConnectedCallback(pc1(), CB(connected1_cb_));
    connected1_cb_.is_registered_ = true;
    mrsPeerConnectionRegisterConnectedCallback(pc2(), CB(connected2_cb_));
    connected2_cb_.is_registered_ = true;
    ASSERT_EQ(Result::kSuccess, mrsPeerConnectionCreateOffer(pc1()));
    ASSERT_EQ(true, ev1.WaitFor(60s));
    ASSERT_EQ(true, ev2.WaitFor(60s));
  }

 protected:
  PCRaii pc1_;
  PCRaii pc2_;
  SdpCallback sdp1_cb_;
  SdpCallback sdp2_cb_;
  IceCallback ice1_cb_;
  IceCallback ice2_cb_;
  InteropCallback<> connected1_cb_;
  InteropCallback<> connected2_cb_;
  void setup() {
    sdp1_cb_ = [this](const char* type, const char* sdp_data) {
      Event ev;
      ASSERT_EQ(Result::kSuccess,
                mrsPeerConnectionSetRemoteDescriptionAsync(
                    pc2_.handle(), type, sdp_data, &SetEventOnCompleted, &ev));
      ev.Wait();
      if (kOfferString == type) {
        ASSERT_EQ(Result::kSuccess,
                  mrsPeerConnectionCreateAnswer(pc2_.handle()));
      }
    };
    sdp2_cb_ = [this](const char* type, const char* sdp_data) {
      Event ev;
      ASSERT_EQ(Result::kSuccess,
                mrsPeerConnectionSetRemoteDescriptionAsync(
                    pc1_.handle(), type, sdp_data, &SetEventOnCompleted, &ev));
      ev.Wait();
      if (kOfferString == type) {
        ASSERT_EQ(Result::kSuccess,
                  mrsPeerConnectionCreateAnswer(pc1_.handle()));
      }
    };
    ice1_cb_ = [this](const char* candidate, int sdpMlineindex,
                      const char* sdpMid) {
      ASSERT_EQ(Result::kSuccess,
                mrsPeerConnectionAddIceCandidate(pc2_.handle(), sdpMid,
                                                 sdpMlineindex, candidate));
    };
    ice2_cb_ = [this](const char* candidate, int sdpMlineindex,
                      const char* sdpMid) {
      ASSERT_EQ(Result::kSuccess,
                mrsPeerConnectionAddIceCandidate(pc1_.handle(), sdpMid,
                                                 sdpMlineindex, candidate));
    };
  }

  void shutdown() {
    if (connected1_cb_.is_registered_) {
      mrsPeerConnectionRegisterConnectedCallback(pc1(), nullptr, nullptr);
      connected1_cb_.is_registered_ = false;
    }
    if (connected2_cb_.is_registered_) {
      mrsPeerConnectionRegisterConnectedCallback(pc2(), nullptr, nullptr);
      connected2_cb_.is_registered_ = false;
    }
  }

  static void SetEventOnCompleted(void* user_data) {
    Event* ev = (Event*)user_data;
    ev->Set();
  }
};

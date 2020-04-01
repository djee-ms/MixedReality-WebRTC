// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

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
  std::mutex m_;
  std::condition_variable cv_;
  bool signaled_{false};
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
    create(config);
  }
  /// Create a peer connection with a specific configuration.
  PCRaii(const PeerConnectionConfiguration& config) { create(config); }
  ~PCRaii() = default;
  const std::shared_ptr<PeerConnection>& pc() const { return pc_; }

 protected:
  std::shared_ptr<PeerConnection> pc_;
  void create(const PeerConnectionConfiguration& config) {
    pc_ = PeerConnection::Create(config);
    ASSERT_NE(nullptr, pc_);
  }
};

constexpr const std::string_view kOfferString{"offer"};

/// Helper to create a pair of peer connections and locally connect them to each
/// other via simple hard-coded signaling.
class LocalPeerPairRaii {
 public:
  LocalPeerPairRaii() { setup(); }
  LocalPeerPairRaii(const PeerConnectionConfiguration& config)
      : pc1_(config), pc2_(config) {
    setup();
  }
  ~LocalPeerPairRaii() { shutdown(); }

  const std::shared_ptr<PeerConnection>& pc1() const { return pc1_.pc(); }
  const std::shared_ptr<PeerConnection>& pc2() const { return pc2_.pc(); }

  void ConnectAndWait() {
    Event ev1, ev2;
    pc1()->RegisterConnectedCallback([&ev1]() { ev1.Set(); });
    pc2()->RegisterConnectedCallback([&ev2]() { ev2.Set(); });
    std::future<SdpDescription> future_desc = pc1()->CreateOfferAsync();
    ASSERT_EQ(std::future_status::ready, future_desc.wait_for(60s));
    ASSERT_EQ(SdpType::kOffer, future_desc.get().type);
    ASSERT_EQ(true, ev1.WaitFor(60s));
    ASSERT_EQ(true, ev2.WaitFor(60s));
  }

 protected:
  PCRaii pc1_;
  PCRaii pc2_;
  std::function<void(const SdpDescription&)> sdp1_cb_;
  std::function<void(const SdpDescription&)> sdp2_cb_;
  std::function<void(const IceCandidate&)> ice1_cb_;
  std::function<void(const IceCandidate&)> ice2_cb_;
  void setup() {
    sdp1_cb_ = [this](const SdpDescription& desc) {
      // Send to remote:
      pc2()->SetRemoteDescriptionAsync(desc).wait_for(10s);
      // Remote might initiate reply:
      if (desc.type == SdpType::kOffer) {
        pc2()->CreateAnswerAsync();
      }
    };
    sdp2_cb_ = [this](const SdpDescription& desc) {
      // Send to remote:
      pc1()->SetRemoteDescriptionAsync(desc).wait_for(10s);
      // Remote might initiate reply:
      if (desc.type == SdpType::kOffer) {
        pc1()->CreateAnswerAsync();
      }
    };
    ice1_cb_ = [this](const IceCandidate& candidate) {
      // Send to remote:
      pc2()->AddIceCandidate(candidate);
    };
    ice2_cb_ = [this](const IceCandidate& candidate) {
      // Send to remote:
      pc1()->AddIceCandidate(candidate);
    };
  }

  void shutdown() {}
};

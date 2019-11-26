
#include "data_channel.h"
#include "local_video_track.h"
#include "peer_connection.h"

#include <condition_variable>
#include <mutex>

using namespace std::chrono_literals;
using namespace Microsoft::MixedReality::WebRTC;

namespace {

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

template <typename... Args>
class CallbackEx : public Callback<Args...> {
 public:
  using func_type = std::function<void(Args...)>;
  func_type func_;
  CallbackEx(func_type&& func) : func_(std::move(func)) {
    this->callback_ = &Invoke;
    this->user_data_ = this;
  }
  static void MRS_CALL Invoke(void* user_data, Args... args) noexcept {
    auto ptr = static_cast<CallbackEx*>(user_data);
    (ptr->func_)(args...);
  }
};

using SdpCallback = CallbackEx<const char*, const char*>;
using IceCallback = CallbackEx<const char*, int, const char*>;
using ConnectCallback = CallbackEx<>;

}  // namespace

int main(int, const char*[]) {
  PeerConnectionConfiguration config{};
  mrsPeerConnectionInteropHandle h1{}, h2{};
  RefPtr<PeerConnection> pc1 = PeerConnection::create(config, h1).value();
  RefPtr<PeerConnection> pc2 = PeerConnection::create(config, h2).value();
  SdpCallback cb_sdp1([&pc2](const char* type, const char* sdp) {
    pc2->SetRemoteDescription(type, sdp);
    if (strcmp(type, "offer") == 0) {
      pc2->CreateAnswer();
    }
  });
  SdpCallback cb_sdp2([&pc1](const char* type, const char* sdp) {
    pc1->SetRemoteDescription(type, sdp);
    if (strcmp(type, "offer") == 0) {
      pc1->CreateAnswer();
    }
  });
  pc1->RegisterLocalSdpReadytoSendCallback(Callback(cb_sdp1));
  pc2->RegisterLocalSdpReadytoSendCallback(Callback(cb_sdp2));
  IceCallback cb_ice1(
      [&pc2](const char* candidate, int sdpMlineIndex, const char* sdpMid) {
        pc2->AddIceCandidate(sdpMid, sdpMlineIndex, candidate);
      });
  IceCallback cb_ice2(
      [&pc1](const char* candidate, int sdpMlineIndex, const char* sdpMid) {
        pc1->AddIceCandidate(sdpMid, sdpMlineIndex, candidate);
      });
  pc1->RegisterIceCandidateReadytoSendCallback(Callback(cb_ice1));
  pc2->RegisterIceCandidateReadytoSendCallback(Callback(cb_ice2));
  Event ev1, ev2;
  ConnectCallback cb_con1([&ev1]() { ev1.Set(); });
  ConnectCallback cb_con2([&ev2]() { ev2.Set(); });
  pc1->RegisterConnectedCallback(Callback(cb_con1));
  pc2->RegisterConnectedCallback(Callback(cb_con2));
  pc1->CreateOffer();
  ev1.WaitFor(5s);
  ev2.WaitFor(5s);
  pc1->RegisterLocalSdpReadytoSendCallback({});
  pc2->RegisterLocalSdpReadytoSendCallback({});
  pc1->RegisterIceCandidateReadytoSendCallback({});
  pc2->RegisterIceCandidateReadytoSendCallback({});
  pc1->RegisterConnectedCallback({});
  pc2->RegisterConnectedCallback({});
}

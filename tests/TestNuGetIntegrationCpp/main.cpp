
#include "data_channel.h"
#include "local_video_track.h"
#include "peer_connection.h"

#include <condition_variable>
#include <mutex>

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

}  // namespace

int main(int argc, const char* argv[]) {
  PeerConnectionConfiguration config{};
  RefPtr<PeerConnection> pc1 = PeerConnection::create(config);
  RefPtr<PeerConnection> pc2 = PeerConnection::create(config);
  pc1->RegisterLocalSdpReadytoSendCallback(
      [&pc2](const char* type, const char* sdp) {
        pc2->SetRemoteDescription(type, sdp);
        if (strcmp(type, "offer") == 0) {
          pc2->CreateAnswer();
        }
      });
  pc2->RegisterLocalSdpReadytoSendCallback(
      [&pc1](const char* type, const char* sdp) {
        pc1->SetRemoteDescription(type, sdp);
        if (strcmp(type, "offer") == 0) {
          pc1->CreateAnswer();
        }
      });
  pc1->RegisterIceCandidateReadytoSendCallback(
      [&pc2](const char* candidate, int sdpMlineIndex, const char* sdpMid) {
        pc2->AddIceCandidate(sdpMid, sdpMlineIndex, candidate);
      });
  pc2->RegisterIceCandidateReadytoSendCallback(
      [&pc1](const char* candidate, int sdpMlineIndex, const char* sdpMid) {
        pc1->AddIceCandidate(sdpMid, sdpMlineIndex, candidate);
      });
  Event ev1, ev2;
  pc1->RegisterConnectedCallback([&ev1]() { ev1.Set(); });
  pc2->RegisterConnectedCallback([&ev2]() { ev2.Set(); });
  pc1->CreateOffer();
  ev1.WaitFor(5s);
  ev2.WaitFor(5s);
}

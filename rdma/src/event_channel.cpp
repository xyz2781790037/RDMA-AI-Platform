#include "rdma/event_channel.h"

namespace rdma {

void EventChannel::Notify() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    signaled_ = true;
  }
  cv_.notify_all();
}

bool EventChannel::Wait(std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!cv_.wait_for(lock, timeout, [&] { return signaled_; })) {
    return false;
  }
  signaled_ = false;
  return true;
}

void EventChannel::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  signaled_ = false;
}

}  // namespace rdma

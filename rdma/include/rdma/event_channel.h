#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace rdma {

class EventChannel {
 public:
  void Notify();
  bool Wait(std::chrono::milliseconds timeout);
  void Reset();

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool signaled_{false};
};

}  // namespace rdma

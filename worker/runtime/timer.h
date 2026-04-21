#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace worker {

class Timer {
 public:
  Timer() = default;
  ~Timer();

  bool start(std::chrono::milliseconds interval, std::function<void()> task);
  void stop();

 private:
  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool running_ = false;
};

}  // namespace worker

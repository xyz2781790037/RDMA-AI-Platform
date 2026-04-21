#include "timer.h"

#include <exception>
#include <string>
#include <utility>

#include "logger.h"

namespace worker {

Timer::~Timer() {
  stop();
}

bool Timer::start(std::chrono::milliseconds interval, std::function<void()> task) {
  if (interval.count() <= 0 || !task) {
    return false;
  }

  stop();

  {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
  }

  thread_ = std::thread([this, interval, task = std::move(task)]() mutable {
    std::unique_lock<std::mutex> lock(mutex_);
    while (running_) {
      if (condition_.wait_for(lock, interval, [this] { return !running_; })) {
        break;
      }

      lock.unlock();
      try {
        task();
      } catch (const std::exception& ex) {
        ERROR("timer task failed: " + std::string(ex.what()));
      } catch (...) {
        ERROR("timer task failed: unknown exception");
      }
      lock.lock();
    }
  });

  return true;
}

void Timer::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ && !thread_.joinable()) {
      return;
    }
    running_ = false;
  }

  condition_.notify_all();

  if (thread_.joinable()) {
    thread_.join();
  }
}

}  // namespace worker

#include "thread_pool.h"

#include <exception>
#include <string>
#include <utility>

#include "logger.h"

namespace worker {

ThreadPool::ThreadPool(std::size_t thread_count) : thread_count_(thread_count) {}

ThreadPool::~ThreadPool() {
  stop();
}

bool ThreadPool::start() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) {
      return true;
    }

    if (thread_count_ == 0) {
      ERROR("thread pool start failed: thread count must be greater than zero");
      return false;
    }

    stopping_ = false;
  }

  try {
    for (std::size_t i = 0; i < thread_count_; ++i) {
      threads_.emplace_back(&ThreadPool::workerLoop, this);
    }
  } catch (const std::exception& ex) {
    ERROR("thread pool start failed: " + std::string(ex.what()));
    stop();
    return false;
  } catch (...) {
    ERROR("thread pool start failed: unknown exception");
    stop();
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  started_ = true;
  return true;
}

bool ThreadPool::enqueue(std::function<void()> task) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!started_ || stopping_) {
    return false;
  }

  tasks_.push(std::move(task));
  condition_.notify_one();
  return true;
}

void ThreadPool::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_ && threads_.empty()) {
      return;
    }
    stopping_ = true;
  }

  condition_.notify_all();

  for (std::thread& thread : threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  std::lock_guard<std::mutex> lock(mutex_);
  threads_.clear();
  while (!tasks_.empty()) {
    tasks_.pop();
  }
  started_ = false;
}

void ThreadPool::workerLoop() {
  while (true) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });

      if (stopping_ && tasks_.empty()) {
        return;
      }

      task = std::move(tasks_.front());
      tasks_.pop();
    }

    try {
      task();
    } catch (const std::exception& ex) {
      ERROR("thread pool task failed: " + std::string(ex.what()));
    } catch (...) {
      ERROR("thread pool task failed: unknown exception");
    }
  }
}

}  // namespace worker

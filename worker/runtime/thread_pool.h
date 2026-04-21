#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace worker {

class ThreadPool {
 public:
  explicit ThreadPool(std::size_t thread_count);
  ~ThreadPool();

  bool start();
  bool enqueue(std::function<void()> task);
  void stop();

 private:
  void workerLoop();

  std::size_t thread_count_;
  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool started_ = false;
  bool stopping_ = false;
};

}  // namespace worker

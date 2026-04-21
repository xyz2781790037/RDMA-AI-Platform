#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>

#include "task.h"

namespace worker {

class TaskQueue {
 public:
  bool push(Task task);
  bool waitPop(Task& task);
  void stop();

 private:
  std::deque<Task> queue_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool stopped_ = false;
};

}  // namespace worker

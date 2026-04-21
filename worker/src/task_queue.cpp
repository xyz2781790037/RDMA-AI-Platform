#include "task_queue.h"

#include <utility>

namespace worker {

bool TaskQueue::push(Task task) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_) {
      return false;
    }
    queue_.push_back(std::move(task));
  }

  condition_.notify_one();
  return true;
}

bool TaskQueue::waitPop(Task& task) {
  std::unique_lock<std::mutex> lock(mutex_);
  condition_.wait(lock, [this] { return stopped_ || !queue_.empty(); });

  if (stopped_) {
    return false;
  }

  task = std::move(queue_.front());
  queue_.pop_front();
  return true;
}

void TaskQueue::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
  }

  condition_.notify_all();
}

}  // namespace worker

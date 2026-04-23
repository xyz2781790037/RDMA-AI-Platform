#include "rdma/completion_queue.h"

#include <algorithm>
#include <utility>

#include "rdma/event_channel.h"

namespace rdma {

CompletionQueue::CompletionQueue(CompletionQueueConfig config)
    : config_(std::move(config)) {}

void CompletionQueue::Bind(EventChannel& channel) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (std::find(channels_.begin(), channels_.end(), &channel) == channels_.end()) {
    channels_.push_back(&channel);
  }
}

void CompletionQueue::Push(CompletionEvent event) {
  std::vector<EventChannel*> channels;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config_.depth > 0 && queue_.size() >= config_.depth) {
      queue_.pop();
    }
    queue_.push(std::move(event));
    channels = channels_;
  }

  cv_.notify_one();
  for (EventChannel* channel : channels) {
    if (channel != nullptr) {
      channel->Notify();
    }
  }
}

std::optional<CompletionEvent> CompletionQueue::Poll(
    std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!cv_.wait_for(lock, timeout, [&] { return !queue_.empty(); })) {
    return std::nullopt;
  }

  CompletionEvent event = queue_.front();
  queue_.pop();
  return event;
}

std::size_t CompletionQueue::Depth() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.size();
}

std::size_t CompletionQueue::Capacity() const { return config_.depth; }

}  // namespace rdma

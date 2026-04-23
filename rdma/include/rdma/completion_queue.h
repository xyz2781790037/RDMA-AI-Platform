#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "rdma/config.h"
#include "rdma/types.h"

namespace rdma {

class EventChannel;

struct CompletionEvent {
  WorkRequestId work_request_id{0};
  CompletionStatus status{CompletionStatus::kSuccess};
  OperationType operation{OperationType::kSend};
  std::string source;
  std::string destination;
  std::string detail;
  TimePoint completed_at{};
  std::size_t bytes{0};
};

class CompletionQueue {
 public:
  explicit CompletionQueue(CompletionQueueConfig config = {});

  void Bind(EventChannel& channel);
  void Push(CompletionEvent event);
  std::optional<CompletionEvent> Poll(std::chrono::milliseconds timeout);
  std::size_t Depth() const;
  std::size_t Capacity() const;

 private:
  CompletionQueueConfig config_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<CompletionEvent> queue_;
  std::vector<EventChannel*> channels_;
};

}  // namespace rdma

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "rdma/completion_queue.h"
#include "rdma/config.h"
#include "rdma/memory_region.h"
#include "rdma/queue_pair.h"

namespace rdma {

struct Message {
  std::string source;
  std::string destination;
  std::string topic;
  std::string description;
  ByteBufferPtr payload;
  WorkRequestId work_request_id{0};
  OperationType operation{OperationType::kSend};

  std::size_t size() const;
};

class TransportRdma {
 public:
  explicit TransportRdma(TransportConfig config = {});

  void Connect(const QueuePair& qp);
  void PostSend(const QueuePair& qp, Message message, CompletionQueue& cq);
  void PostWrite(const QueuePair& qp, const MemoryRegion& region,
                 std::string topic, CompletionQueue& cq,
                 WorkRequestId work_request_id);
  std::optional<Message> PollReceive(const std::string& node_id,
                                     std::chrono::milliseconds timeout);
  std::string BackendName() const;

 private:
  struct BrokerState {
    std::mutex mutex;
    std::condition_variable cv;
    std::unordered_map<std::string, std::deque<Message>> inboxes;
  };

  TransportConfig config_;
  std::shared_ptr<BrokerState> broker_;
};

}  // namespace rdma

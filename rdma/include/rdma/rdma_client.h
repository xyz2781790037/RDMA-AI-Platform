#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include "rdma/completion_queue.h"
#include "rdma/config.h"
#include "rdma/connection.h"
#include "rdma/memory_region.h"
#include "rdma/result_message.h"
#include "rdma/task_message.h"
#include "rdma/transport_rdma.h"

namespace rdma {

class RdmaClient {
 public:
  RdmaClient(ClientConfig config, std::shared_ptr<TransportRdma> transport);

  bool Connect();
  WorkRequestId SubmitTask(const TaskMessage& task);
  std::optional<ResultMessage> PollResult(std::chrono::milliseconds timeout);
  std::optional<CompletionEvent> PollCompletion(
      std::chrono::milliseconds timeout);
  CompletionQueue& completion_queue();
  MemoryRegistry& memory_registry();
  const ClientConfig& config() const;

 private:
  ClientConfig config_;
  std::shared_ptr<TransportRdma> transport_;
  Connection connection_;
  MemoryRegistry registry_;
  CompletionQueue completions_;
  WorkRequestId next_work_request_id_{1};
};

}  // namespace rdma

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "rdma/buffer_pool.h"
#include "rdma/completion_queue.h"
#include "rdma/connection.h"
#include "rdma/config.h"
#include "rdma/metrics.h"
#include "rdma/result_message.h"
#include "rdma/task_message.h"
#include "rdma/transport_rdma.h"

namespace rdma {

struct SubmissionResult {
  std::string task_id;
  bool success{false};
  std::string detail;
  WorkRequestId work_request_id{0};
};

class WorkerSession {
 public:
  WorkerSession(WorkerSessionConfig config,
                std::shared_ptr<TransportRdma> transport);

  SubmissionResult Submit(const TaskMessage& task);
  std::optional<TaskMessage> PollTask(std::chrono::milliseconds timeout);
  bool SendResult(const ResultMessage& result);
  std::optional<CompletionEvent> PollCompletion(
      std::chrono::milliseconds timeout);
  MetricsSnapshot metrics() const;
  const std::string& node_id() const;

 private:
  Connection& EnsurePeer(const std::string& peer);

  WorkerSessionConfig config_;
  std::shared_ptr<TransportRdma> transport_;
  MemoryRegistry registry_;
  BufferPool buffer_pool_;
  CompletionQueue completions_;
  Metrics metrics_;
  std::unordered_map<std::string, Connection> peers_;
  WorkRequestId next_work_request_id_{1};
};

}  // namespace rdma

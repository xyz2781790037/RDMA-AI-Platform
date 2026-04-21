#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "rdma/completion.h"
#include "rdma/memory.h"
#include "rdma/qp.h"
#include "rdma/transport.h"
#include "worker/task/task.h"

namespace worker::runtime {

class WorkerRuntime {
 public:
  WorkerRuntime(std::string node_id, std::shared_ptr<rdma::Transport> transport);

  task::TaskResult Submit(const task::TaskSpec& spec);
  std::optional<rdma::Message> Poll(std::chrono::milliseconds timeout);
  std::optional<rdma::CompletionEvent> PollCompletion(
      std::chrono::milliseconds timeout);
  const std::string& node_id() const;

 private:
  rdma::QueuePair& EnsurePeer(const std::string& peer);

  std::string node_id_;
  std::shared_ptr<rdma::Transport> transport_;
  rdma::MemoryRegistry registry_;
  rdma::CompletionQueue completions_;
  task::TaskExecutor executor_;
  std::unordered_map<std::string, rdma::QueuePair> peers_;
};

}  // namespace worker::runtime

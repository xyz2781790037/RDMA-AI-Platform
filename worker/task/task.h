#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "rdma/completion.h"
#include "rdma/memory.h"
#include "rdma/qp.h"
#include "rdma/transport.h"
#include "worker/model/tensor.h"

namespace worker::task {

enum class TaskKind {
  kBroadcast,
  kAllReduce,
  kLoadShard,
};

std::string ToString(TaskKind kind);

struct TaskSpec {
  std::string id;
  TaskKind kind{TaskKind::kBroadcast};
  std::string target_node;
  model::Tensor tensor;
  std::map<std::string, std::string> metadata;
};

struct TaskResult {
  std::string task_id;
  bool success{false};
  std::string detail;
  std::uint64_t work_request_id{0};
};

class TaskExecutor {
 public:
  explicit TaskExecutor(std::string local_node);

  TaskResult Execute(const TaskSpec& spec, rdma::QueuePair& qp,
                     rdma::Transport& transport, rdma::MemoryRegistry& registry,
                     rdma::CompletionQueue& completions);

 private:
  std::string local_node_;
  std::uint64_t next_work_request_id_{1};
};

}  // namespace worker::task

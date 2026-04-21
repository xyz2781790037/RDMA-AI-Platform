#include "worker/task/task.h"

#include <utility>

namespace worker::task {

std::string ToString(TaskKind kind) {
  switch (kind) {
    case TaskKind::kBroadcast:
      return "broadcast";
    case TaskKind::kAllReduce:
      return "all-reduce";
    case TaskKind::kLoadShard:
      return "load-shard";
  }
  return "unknown";
}

TaskExecutor::TaskExecutor(std::string local_node)
    : local_node_(std::move(local_node)) {}

TaskResult TaskExecutor::Execute(const TaskSpec& spec, rdma::QueuePair& qp,
                                 rdma::Transport& transport,
                                 rdma::MemoryRegistry& registry,
                                 rdma::CompletionQueue& completions) {
  auto region = registry.Register(spec.tensor.Serialize());
  const auto work_request_id = next_work_request_id_++;

  transport.PostSend(
      qp,
      rdma::Message{
          .source = local_node_,
          .destination = spec.target_node,
          .topic = ToString(spec.kind),
          .description = spec.tensor.Summary(),
          .payload = region.storage(),
          .work_request_id = work_request_id,
      },
      completions);

  return TaskResult{
      .task_id = spec.id,
      .success = true,
      .detail = "task submitted to transport",
      .work_request_id = work_request_id,
  };
}

}  // namespace worker::task

#include <chrono>
#include <iostream>

#include "rdma/result_message.h"
#include "rdma/task_message.h"
#include "rdma/transport_rdma.h"
#include "rdma/worker_session.h"

int main() {
  using namespace std::chrono_literals;

  auto transport = std::make_shared<rdma::TransportRdma>();
  rdma::WorkerSession controller({
      .node_id = "controller",
  }, transport);
  rdma::WorkerSession worker({
      .node_id = "worker-a",
  }, transport);

  const auto submitted = controller.Submit(rdma::TaskMessage{
      .task_id = "job-42",
      .kind = "broadcast",
      .source_node = "controller",
      .target_node = "worker-a",
      .description = "ship one tensor shard",
      .metadata = {{"tensor", "layer-12"}, {"dtype", "fp16"}},
  });

  const auto completion = controller.PollCompletion(1s);
  const auto task = worker.PollTask(1s);
  if (!submitted.success || !completion.has_value() || !task.has_value()) {
    std::cerr << "worker transport demo failed during task delivery\n";
    return 1;
  }

  worker.SendResult(rdma::ResultMessage{
      .task_id = task->task_id,
      .worker_node = worker.node_id(),
      .target_node = "controller",
      .success = true,
      .detail = "task executed by worker session",
      .metadata = {{"status", "done"}},
  });

  const auto raw_result = transport->PollReceive("controller", 1s);
  if (!raw_result.has_value() || raw_result->payload == nullptr) {
    std::cerr << "worker transport demo failed during result delivery\n";
    return 1;
  }

  const rdma::ResultMessage result = rdma::ResultMessage::Deserialize(raw_result->payload);
  const rdma::MetricsSnapshot worker_metrics = worker.metrics();

  std::cout << "worker transport demo\n";
  std::cout << "task_id: " << task->task_id << "\n";
  std::cout << "worker: " << result.worker_node << "\n";
  std::cout << "detail: " << result.detail << "\n";
  std::cout << "worker_receives: " << worker_metrics.receives << "\n";
  std::cout << "worker_sends: " << worker_metrics.sends << "\n";
  return result.success ? 0 : 1;
}

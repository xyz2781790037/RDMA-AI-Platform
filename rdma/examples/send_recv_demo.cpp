#include <chrono>
#include <iostream>

#include "rdma/task_message.h"
#include "rdma/worker_session.h"

int main() {
  using namespace std::chrono_literals;

  auto transport = std::make_shared<rdma::TransportRdma>();
  rdma::WorkerSession sender({
      .node_id = "demo-sender",
  }, transport);
  rdma::WorkerSession receiver({
      .node_id = "demo-receiver",
  }, transport);

  const rdma::SubmissionResult submitted = sender.Submit(rdma::TaskMessage{
      .task_id = "task-1",
      .kind = "send_recv",
      .source_node = "demo-sender",
      .target_node = "demo-receiver",
      .description = "hello from sender",
      .metadata = {{"payload_bytes", "18"}},
  });

  const auto completion = sender.PollCompletion(1s);
  const auto message = receiver.PollTask(1s);

  if (!submitted.success || !completion.has_value() || !message.has_value()) {
    std::cerr << "send/recv demo failed\n";
    return 1;
  }

  std::cout << "send/recv demo\n";
  std::cout << "task_id: " << message->task_id << "\n";
  std::cout << "kind: " << message->kind << "\n";
  std::cout << "from: " << message->source_node << "\n";
  std::cout << "to: " << message->target_node << "\n";
  std::cout << "detail: " << message->description << "\n";
  std::cout << "completion_status: " << rdma::ToString(completion->status) << "\n";
  return 0;
}

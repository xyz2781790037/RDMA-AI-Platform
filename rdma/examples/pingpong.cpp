#include <chrono>
#include <iostream>
#include <thread>

#include "rdma/rdma_client.h"
#include "rdma/rdma_server.h"

int main() {
  using namespace std::chrono_literals;

  constexpr int kRounds = 128;

  auto transport = std::make_shared<rdma::TransportRdma>();
  rdma::RdmaServer server({
      .node_id = "pingpong-server",
  }, transport);
  rdma::RdmaClient client({
      .node_id = "pingpong-client",
      .server_node = "pingpong-server",
  }, transport);

  if (!server.Accept("pingpong-client") || !client.Connect()) {
    std::cerr << "failed to establish pingpong pair\n";
    return 1;
  }

  std::thread server_thread([&] {
    for (int i = 0; i < kRounds; ++i) {
      const auto task = server.PollTask(2s);
      if (!task.has_value()) {
        return;
      }

      server.SendResult(
          "pingpong-client",
          rdma::ResultMessage{
              .task_id = task->task_id,
              .worker_node = server.node_id(),
              .target_node = "pingpong-client",
              .success = true,
              .detail = "pong",
              .metadata = {{"round", std::to_string(i + 1)}},
          });
    }
  });

  int received = 0;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < kRounds; ++i) {
    const auto work_request_id = client.SubmitTask(rdma::TaskMessage{
        .task_id = "ping-" + std::to_string(i + 1),
        .kind = "pingpong",
        .source_node = "pingpong-client",
        .target_node = "pingpong-server",
        .description = "ping",
        .metadata = {{"round", std::to_string(i + 1)}},
    });

    if (work_request_id == 0) {
      break;
    }

    const auto result = client.PollResult(2s);
    if (!result.has_value()) {
      break;
    }
    ++received;
  }
  const auto stop = std::chrono::steady_clock::now();

  server_thread.join();

  const double total_us =
      std::chrono::duration<double, std::micro>(stop - start).count();

  std::cout << "pingpong demo\n";
  std::cout << "rounds: " << kRounds << "\n";
  std::cout << "received: " << received << "\n";
  std::cout << "avg_rtt_us: " << (total_us / kRounds) << "\n";
  return received == kRounds ? 0 : 1;
}

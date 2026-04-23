#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "rdma/completion_queue.h"
#include "rdma/memory_region.h"
#include "rdma/queue_pair.h"
#include "rdma/transport_rdma.h"

namespace {

rdma::QueuePair ReadyQueuePair(const std::string& local,
                               const std::string& remote) {
  rdma::QueuePair qp({
      .local_node = local,
      .remote_node = remote,
      .max_send_wr = 256,
      .max_recv_wr = 256,
  });
  qp.BringUp();
  return qp;
}

}  // namespace

int main() {
  using namespace std::chrono_literals;

  constexpr int kConnections = 8;
  constexpr int kRoundsPerConnection = 512;
  constexpr std::size_t kPayloadBytes = 64 * 1024;

  rdma::TransportRdma transport;
  rdma::CompletionQueue completions;
  rdma::MemoryRegistry registry;
  rdma::MemoryRegion region = registry.Register(kPayloadBytes);
  std::fill(region.storage()->begin(), region.storage()->end(), std::byte{0x11});

  std::vector<rdma::QueuePair> queue_pairs;
  queue_pairs.reserve(kConnections);
  for (int i = 0; i < kConnections; ++i) {
    queue_pairs.push_back(ReadyQueuePair(
        "multi-src-" + std::to_string(i),
        "multi-dst-" + std::to_string(i)));
    transport.Connect(queue_pairs.back());
  }

  std::atomic<int> received{0};
  std::vector<std::thread> receivers;
  receivers.reserve(kConnections);
  for (int i = 0; i < kConnections; ++i) {
    const std::string node = "multi-dst-" + std::to_string(i);
    receivers.emplace_back([&, node] {
      for (int round = 0; round < kRoundsPerConnection; ++round) {
        const auto message = transport.PollReceive(node, 5s);
        if (!message.has_value()) {
          return;
        }
        ++received;
      }
    });
  }

  const auto start = std::chrono::steady_clock::now();
  for (int round = 0; round < kRoundsPerConnection; ++round) {
    for (int i = 0; i < kConnections; ++i) {
      transport.PostSend(
          queue_pairs[static_cast<std::size_t>(i)],
          rdma::Message{
              .source = "multi-src-" + std::to_string(i),
              .destination = "multi-dst-" + std::to_string(i),
              .topic = "multi-conn",
              .description = "64KB fanout",
              .payload = region.storage(),
              .work_request_id = static_cast<rdma::WorkRequestId>(
                  round * kConnections + i + 1),
              .operation = rdma::OperationType::kSend,
          },
          completions);
    }
  }
  for (auto& receiver : receivers) {
    receiver.join();
  }
  const auto stop = std::chrono::steady_clock::now();

  const int total_messages = kConnections * kRoundsPerConnection;
  const double seconds = std::chrono::duration<double>(stop - start).count();
  const double gigabytes =
      static_cast<double>(total_messages) * static_cast<double>(kPayloadBytes) /
      (1024.0 * 1024.0 * 1024.0);

  std::cout << "RDMA multi-connection benchmark\n";
  std::cout << "connections: " << kConnections << "\n";
  std::cout << "messages: " << total_messages << "\n";
  std::cout << "payload_bytes: " << kPayloadBytes << "\n";
  std::cout << "received: " << received.load() << "\n";
  std::cout << "aggregate_gbps: " << (gigabytes / seconds) << "\n";
  return received.load() == total_messages ? 0 : 1;
}

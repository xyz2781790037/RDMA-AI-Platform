#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>

#include "rdma/completion.h"
#include "rdma/memory.h"
#include "rdma/qp.h"
#include "rdma/transport.h"

namespace {

rdma::QueuePair ReadyQueuePair(const std::string& local,
                               const std::string& remote) {
  rdma::QueuePair qp({
      .local_node = local,
      .remote_node = remote,
      .max_send_wr = 512,
      .max_recv_wr = 512,
  });
  qp.TransitionTo(rdma::QueuePairState::kInit);
  qp.TransitionTo(rdma::QueuePairState::kReadyToReceive);
  qp.TransitionTo(rdma::QueuePairState::kReadyToSend);
  return qp;
}

}  // namespace

int main() {
  using namespace std::chrono_literals;

  constexpr int kRounds = 1024;
  constexpr std::size_t kPayloadBytes = 1 * 1024 * 1024;

  rdma::Transport transport;
  rdma::CompletionQueue completions;
  rdma::MemoryRegistry registry;
  rdma::QueuePair qp = ReadyQueuePair("throughput-src", "throughput-dst");
  transport.Connect(qp);

  auto region = registry.Register(kPayloadBytes);
  std::fill(region.storage()->begin(), region.storage()->end(), std::byte{0x5A});

  std::atomic<int> received{0};
  std::thread receiver([&] {
    for (int i = 0; i < kRounds; ++i) {
      auto message = transport.PollReceive("throughput-dst", 5s);
      if (!message.has_value()) {
        return;
      }
      ++received;
    }
  });

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < kRounds; ++i) {
    transport.PostSend(
        qp,
        rdma::Message{
            .source = "throughput-src",
            .destination = "throughput-dst",
            .topic = "throughput",
            .description = "1MB block",
            .payload = region.storage(),
            .work_request_id = static_cast<std::uint64_t>(i + 1),
        },
        completions);
  }
  receiver.join();
  const auto stop = std::chrono::steady_clock::now();

  const double seconds =
      std::chrono::duration<double>(stop - start).count();
  const double gigabytes =
      static_cast<double>(kRounds) * static_cast<double>(kPayloadBytes) /
      (1024.0 * 1024.0 * 1024.0);

  std::cout << "RDMA throughput benchmark\n";
  std::cout << "backend: " << transport.BackendName() << "\n";
  std::cout << "rounds: " << kRounds << "\n";
  std::cout << "payload_bytes: " << kPayloadBytes << "\n";
  std::cout << "received: " << received.load() << "\n";
  std::cout << "throughput_gbps: " << (gigabytes / seconds) << "\n";

  return received.load() == kRounds ? 0 : 1;
}

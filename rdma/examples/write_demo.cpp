#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>

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
      .max_send_wr = 64,
      .max_recv_wr = 64,
  });
  qp.BringUp();
  return qp;
}

}  // namespace

int main() {
  using namespace std::chrono_literals;

  rdma::TransportRdma transport;
  rdma::CompletionQueue completions;
  rdma::MemoryRegistry registry;
  rdma::QueuePair qp = ReadyQueuePair("write-src", "write-dst");
  transport.Connect(qp);

  rdma::MemoryRegion region = registry.Register(4096);
  std::fill(region.storage()->begin(), region.storage()->end(), std::byte{0x3C});

  bool received = false;
  std::thread receiver([&] {
    const auto message = transport.PollReceive("write-dst", 1s);
    if (!message.has_value()) {
      return;
    }
    received = message->operation == rdma::OperationType::kWrite &&
               message->size() == region.size();
  });

  transport.PostWrite(qp, region, "tensor-write", completions, 1);
  const auto completion = completions.Poll(1s);
  receiver.join();

  if (!received || !completion.has_value()) {
    std::cerr << "write demo failed\n";
    return 1;
  }

  std::cout << "write demo\n";
  std::cout << "backend: " << transport.BackendName() << "\n";
  std::cout << "bytes: " << region.size() << "\n";
  std::cout << "completion_status: " << rdma::ToString(completion->status) << "\n";
  return 0;
}

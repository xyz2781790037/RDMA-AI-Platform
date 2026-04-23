#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "rdma/completion_queue.h"

namespace rdma {

struct MetricsSnapshot {
  std::uint64_t sends{0};
  std::uint64_t writes{0};
  std::uint64_t receives{0};
  std::uint64_t completions{0};
  std::uint64_t failed_completions{0};
  std::uint64_t sent_bytes{0};
  std::uint64_t received_bytes{0};
};

class Metrics {
 public:
  void RecordSend(std::size_t bytes);
  void RecordWrite(std::size_t bytes);
  void RecordReceive(std::size_t bytes);
  void RecordCompletion(const CompletionEvent& event);
  MetricsSnapshot Snapshot() const;

 private:
  std::atomic<std::uint64_t> sends_{0};
  std::atomic<std::uint64_t> writes_{0};
  std::atomic<std::uint64_t> receives_{0};
  std::atomic<std::uint64_t> completions_{0};
  std::atomic<std::uint64_t> failed_completions_{0};
  std::atomic<std::uint64_t> sent_bytes_{0};
  std::atomic<std::uint64_t> received_bytes_{0};
};

}  // namespace rdma

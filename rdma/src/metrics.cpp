#include "rdma/metrics.h"

namespace rdma {

void Metrics::RecordSend(std::size_t bytes) {
  sends_.fetch_add(1, std::memory_order_relaxed);
  sent_bytes_.fetch_add(bytes, std::memory_order_relaxed);
}

void Metrics::RecordWrite(std::size_t bytes) {
  writes_.fetch_add(1, std::memory_order_relaxed);
  sent_bytes_.fetch_add(bytes, std::memory_order_relaxed);
}

void Metrics::RecordReceive(std::size_t bytes) {
  receives_.fetch_add(1, std::memory_order_relaxed);
  received_bytes_.fetch_add(bytes, std::memory_order_relaxed);
}

void Metrics::RecordCompletion(const CompletionEvent& event) {
  completions_.fetch_add(1, std::memory_order_relaxed);
  if (event.status != CompletionStatus::kSuccess) {
    failed_completions_.fetch_add(1, std::memory_order_relaxed);
  }
}

MetricsSnapshot Metrics::Snapshot() const {
  return MetricsSnapshot{
      .sends = sends_.load(std::memory_order_relaxed),
      .writes = writes_.load(std::memory_order_relaxed),
      .receives = receives_.load(std::memory_order_relaxed),
      .completions = completions_.load(std::memory_order_relaxed),
      .failed_completions = failed_completions_.load(std::memory_order_relaxed),
      .sent_bytes = sent_bytes_.load(std::memory_order_relaxed),
      .received_bytes = received_bytes_.load(std::memory_order_relaxed),
  };
}

}  // namespace rdma

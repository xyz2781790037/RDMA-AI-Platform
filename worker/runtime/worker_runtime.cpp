#include "worker/runtime/worker_runtime.h"

#include <utility>

namespace worker::runtime {

WorkerRuntime::WorkerRuntime(std::string node_id,
                             std::shared_ptr<rdma::Transport> transport)
    : node_id_(std::move(node_id)),
      transport_(std::move(transport)),
      executor_(node_id_) {}

task::TaskResult WorkerRuntime::Submit(const task::TaskSpec& spec) {
  const std::string peer = spec.target_node.empty() ? node_id_ : spec.target_node;
  auto& qp = EnsurePeer(peer);
  return executor_.Execute(spec, qp, *transport_, registry_, completions_);
}

std::optional<rdma::Message> WorkerRuntime::Poll(
    std::chrono::milliseconds timeout) {
  return transport_->PollReceive(node_id_, timeout);
}

std::optional<rdma::CompletionEvent> WorkerRuntime::PollCompletion(
    std::chrono::milliseconds timeout) {
  return completions_.Poll(timeout);
}

const std::string& WorkerRuntime::node_id() const { return node_id_; }

rdma::QueuePair& WorkerRuntime::EnsurePeer(const std::string& peer) {
  auto it = peers_.find(peer);
  if (it != peers_.end()) {
    return it->second;
  }

  rdma::QueuePair qp({
      .local_node = node_id_,
      .remote_node = peer,
      .max_send_wr = 256,
      .max_recv_wr = 256,
  });
  qp.TransitionTo(rdma::QueuePairState::kInit);
  qp.TransitionTo(rdma::QueuePairState::kReadyToReceive);
  qp.TransitionTo(rdma::QueuePairState::kReadyToSend);
  transport_->Connect(qp);

  auto [inserted, _] = peers_.emplace(peer, std::move(qp));
  return inserted->second;
}

}  // namespace worker::runtime

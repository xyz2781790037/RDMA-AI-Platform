#include "rdma/transport_rdma.h"

#include <utility>

namespace rdma {

std::size_t Message::size() const {
  return payload == nullptr ? 0U : payload->size();
}

TransportRdma::TransportRdma(TransportConfig config)
    : config_(std::move(config)), broker_(std::make_shared<BrokerState>()) {}

void TransportRdma::Connect(const QueuePair& qp) {
  std::lock_guard<std::mutex> lock(broker_->mutex);
  broker_->inboxes.try_emplace(qp.config().local_node);
  broker_->inboxes.try_emplace(qp.config().remote_node);
}

void TransportRdma::PostSend(const QueuePair& qp, Message message,
                             CompletionQueue& cq) {
  const auto now = Clock::now();
  const WorkRequestId work_request_id = message.work_request_id;
  const OperationType operation = message.operation;
  const std::size_t bytes = message.size();

  if (!qp.IsReadyToSend()) {
    cq.Push(CompletionEvent{
        .work_request_id = work_request_id,
        .status = CompletionStatus::kFailed,
        .operation = operation,
        .source = qp.config().local_node,
        .destination = qp.config().remote_node,
        .detail = "queue pair is not ready to send",
        .completed_at = now,
        .bytes = 0,
    });
    return;
  }

  message.source = message.source.empty() ? qp.config().local_node : message.source;
  message.destination = qp.config().remote_node;

  {
    std::lock_guard<std::mutex> lock(broker_->mutex);
    auto& inbox = broker_->inboxes[qp.config().remote_node];
    if (config_.mailbox_capacity > 0 &&
        inbox.size() >= config_.mailbox_capacity) {
      inbox.pop_front();
    }
    inbox.push_back(std::move(message));
  }
  broker_->cv.notify_all();

  cq.Push(CompletionEvent{
      .work_request_id = work_request_id,
      .status = CompletionStatus::kSuccess,
      .operation = operation,
      .source = qp.config().local_node,
      .destination = qp.config().remote_node,
      .detail = "send completed through in-memory RDMA backend",
      .completed_at = now,
      .bytes = bytes,
  });
}

void TransportRdma::PostWrite(const QueuePair& qp, const MemoryRegion& region,
                              std::string topic, CompletionQueue& cq,
                              WorkRequestId work_request_id) {
  PostSend(
      qp,
      Message{
          .source = qp.config().local_node,
          .destination = qp.config().remote_node,
          .topic = std::move(topic),
          .description = region.key(),
          .payload = region.storage(),
          .work_request_id = work_request_id,
          .operation = OperationType::kWrite,
      },
      cq);
}

std::optional<Message> TransportRdma::PollReceive(
    const std::string& node_id, std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(broker_->mutex);
  broker_->inboxes.try_emplace(node_id);

  if (!broker_->cv.wait_for(lock, timeout, [&] {
        const auto it = broker_->inboxes.find(node_id);
        return it != broker_->inboxes.end() && !it->second.empty();
      })) {
    return std::nullopt;
  }

  auto& inbox = broker_->inboxes[node_id];
  Message message = std::move(inbox.front());
  inbox.pop_front();
  return message;
}

std::string TransportRdma::BackendName() const { return config_.backend_name; }

}  // namespace rdma

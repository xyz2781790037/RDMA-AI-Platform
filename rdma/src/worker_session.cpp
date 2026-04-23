#include "rdma/worker_session.h"

#include <utility>

namespace rdma {

WorkerSession::WorkerSession(WorkerSessionConfig config,
                             std::shared_ptr<TransportRdma> transport)
    : config_(std::move(config)),
      transport_(std::move(transport)),
      buffer_pool_(config_.buffer_pool, registry_),
      completions_(config_.completion_queue) {}

SubmissionResult WorkerSession::Submit(const TaskMessage& task) {
  if (transport_ == nullptr) {
    return SubmissionResult{
        .task_id = task.task_id,
        .success = false,
        .detail = "transport is not configured",
        .work_request_id = 0,
    };
  }

  const std::string peer =
      task.target_node.empty() ? config_.node_id : task.target_node;
  Connection& connection = EnsurePeer(peer);
  MemoryRegion region = registry_.Register(task.Serialize());
  const WorkRequestId work_request_id = next_work_request_id_++;

  transport_->PostSend(
      connection.queue_pair(),
      Message{
          .source = config_.node_id,
          .destination = peer,
          .topic = "task",
          .description = task.description,
          .payload = region.storage(),
          .work_request_id = work_request_id,
          .operation = OperationType::kSend,
      },
      completions_);
  metrics_.RecordSend(region.size());

  return SubmissionResult{
      .task_id = task.task_id,
      .success = true,
      .detail = "task submitted through rdma session",
      .work_request_id = work_request_id,
  };
}

std::optional<TaskMessage> WorkerSession::PollTask(
    std::chrono::milliseconds timeout) {
  if (transport_ == nullptr) {
    return std::nullopt;
  }
  std::optional<Message> message = transport_->PollReceive(config_.node_id, timeout);
  if (!message.has_value() || message->payload == nullptr) {
    return std::nullopt;
  }

  metrics_.RecordReceive(message->size());
  return TaskMessage::Deserialize(message->payload);
}

bool WorkerSession::SendResult(const ResultMessage& result) {
  if (transport_ == nullptr) {
    return false;
  }

  const std::string peer =
      result.target_node.empty() ? config_.node_id : result.target_node;
  Connection& connection = EnsurePeer(peer);
  ByteBuffer bytes = result.Serialize();
  auto payload = std::make_shared<ByteBuffer>(bytes);
  const WorkRequestId work_request_id = next_work_request_id_++;

  transport_->PostSend(
      connection.queue_pair(),
      Message{
          .source = config_.node_id,
          .destination = peer,
          .topic = "result",
          .description = result.detail,
          .payload = std::move(payload),
          .work_request_id = work_request_id,
          .operation = OperationType::kResult,
      },
      completions_);
  metrics_.RecordSend(bytes.size());
  return true;
}

std::optional<CompletionEvent> WorkerSession::PollCompletion(
    std::chrono::milliseconds timeout) {
  std::optional<CompletionEvent> event = completions_.Poll(timeout);
  if (event.has_value()) {
    metrics_.RecordCompletion(*event);
  }
  return event;
}

MetricsSnapshot WorkerSession::metrics() const { return metrics_.Snapshot(); }

const std::string& WorkerSession::node_id() const { return config_.node_id; }

Connection& WorkerSession::EnsurePeer(const std::string& peer) {
  const auto it = peers_.find(peer);
  if (it != peers_.end()) {
    return it->second;
  }

  Connection connection(QueuePair({
      .local_node = config_.node_id,
      .remote_node = peer,
      .max_send_wr = 256,
      .max_recv_wr = 256,
  }));
  connection.Establish();
  if (transport_ != nullptr) {
    transport_->Connect(connection.queue_pair());
  }

  auto [inserted, _] = peers_.emplace(peer, std::move(connection));
  return inserted->second;
}

}  // namespace rdma

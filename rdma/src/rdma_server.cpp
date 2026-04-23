#include "rdma/rdma_server.h"

#include <utility>

namespace rdma {

RdmaServer::RdmaServer(ServerConfig config,
                       std::shared_ptr<TransportRdma> transport)
    : config_(std::move(config)), transport_(std::move(transport)) {}

bool RdmaServer::Accept(const std::string& client_node) {
  if (transport_ == nullptr) {
    return false;
  }
  EnsurePeer(client_node);
  return true;
}

std::optional<TaskMessage> RdmaServer::PollTask(
    std::chrono::milliseconds timeout) {
  if (transport_ == nullptr) {
    return std::nullopt;
  }
  std::optional<Message> message = transport_->PollReceive(config_.node_id, timeout);
  if (!message.has_value() || message->payload == nullptr) {
    return std::nullopt;
  }
  return TaskMessage::Deserialize(message->payload);
}

bool RdmaServer::SendResult(const std::string& client_node,
                            const ResultMessage& result) {
  if (transport_ == nullptr) {
    return false;
  }
  Connection& connection = EnsurePeer(client_node);
  auto payload = std::make_shared<ByteBuffer>(result.Serialize());
  transport_->PostSend(
      connection.queue_pair(),
      Message{
          .source = config_.node_id,
          .destination = client_node,
          .topic = "result",
          .description = result.detail,
          .payload = std::move(payload),
          .work_request_id = 0,
          .operation = OperationType::kResult,
      },
      completions_);
  return true;
}

std::optional<CompletionEvent> RdmaServer::PollCompletion(
    std::chrono::milliseconds timeout) {
  return completions_.Poll(timeout);
}

CompletionQueue& RdmaServer::completion_queue() { return completions_; }

const std::string& RdmaServer::node_id() const { return config_.node_id; }

Connection& RdmaServer::EnsurePeer(const std::string& client_node) {
  const auto it = peers_.find(client_node);
  if (it != peers_.end()) {
    return it->second;
  }

  Connection connection(QueuePair({
      .local_node = config_.node_id,
      .remote_node = client_node,
      .max_send_wr = 256,
      .max_recv_wr = 256,
  }));
  connection.Establish();
  if (transport_ != nullptr) {
    transport_->Connect(connection.queue_pair());
  }

  auto [inserted, _] = peers_.emplace(client_node, std::move(connection));
  return inserted->second;
}

}  // namespace rdma

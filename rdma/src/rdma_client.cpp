#include "rdma/rdma_client.h"

#include <utility>

#include "rdma/handshake.h"

namespace rdma {

RdmaClient::RdmaClient(ClientConfig config,
                       std::shared_ptr<TransportRdma> transport)
    : config_(std::move(config)),
      transport_(std::move(transport)),
      connection_(QueuePair({
          .local_node = config_.node_id,
          .remote_node = config_.server_node,
          .max_send_wr = 256,
          .max_recv_wr = 256,
      })) {}

bool RdmaClient::Connect() {
  if (transport_ == nullptr) {
    return false;
  }

  const HandshakeRequest request = Handshake::Build(connection_.queue_pair().config());
  const HandshakeResponse response = Handshake::Validate(request);
  if (!response.accepted) {
    return false;
  }

  if (!connection_.Establish()) {
    return false;
  }

  transport_->Connect(connection_.queue_pair());
  return true;
}

WorkRequestId RdmaClient::SubmitTask(const TaskMessage& task) {
  if (!connection_.established() || transport_ == nullptr) {
    return 0;
  }

  MemoryRegion region = registry_.Register(task.Serialize());
  const WorkRequestId work_request_id = next_work_request_id_++;
  transport_->PostSend(
      connection_.queue_pair(),
      Message{
          .source = config_.node_id,
          .destination = config_.server_node,
          .topic = "task",
          .description = task.description,
          .payload = region.storage(),
          .work_request_id = work_request_id,
          .operation = OperationType::kSend,
      },
      completions_);
  return work_request_id;
}

std::optional<ResultMessage> RdmaClient::PollResult(
    std::chrono::milliseconds timeout) {
  if (transport_ == nullptr) {
    return std::nullopt;
  }
  std::optional<Message> message = transport_->PollReceive(config_.node_id, timeout);
  if (!message.has_value() || message->payload == nullptr) {
    return std::nullopt;
  }
  return ResultMessage::Deserialize(message->payload);
}

std::optional<CompletionEvent> RdmaClient::PollCompletion(
    std::chrono::milliseconds timeout) {
  return completions_.Poll(timeout);
}

CompletionQueue& RdmaClient::completion_queue() { return completions_; }

MemoryRegistry& RdmaClient::memory_registry() { return registry_; }

const ClientConfig& RdmaClient::config() const { return config_; }

}  // namespace rdma

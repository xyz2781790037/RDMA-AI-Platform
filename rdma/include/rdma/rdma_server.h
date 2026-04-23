#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "rdma/completion_queue.h"
#include "rdma/config.h"
#include "rdma/connection.h"
#include "rdma/result_message.h"
#include "rdma/task_message.h"
#include "rdma/transport_rdma.h"

namespace rdma {

class RdmaServer {
 public:
  RdmaServer(ServerConfig config, std::shared_ptr<TransportRdma> transport);

  bool Accept(const std::string& client_node);
  std::optional<TaskMessage> PollTask(std::chrono::milliseconds timeout);
  bool SendResult(const std::string& client_node, const ResultMessage& result);
  std::optional<CompletionEvent> PollCompletion(
      std::chrono::milliseconds timeout);
  CompletionQueue& completion_queue();
  const std::string& node_id() const;

 private:
  Connection& EnsurePeer(const std::string& client_node);

  ServerConfig config_;
  std::shared_ptr<TransportRdma> transport_;
  CompletionQueue completions_;
  std::unordered_map<std::string, Connection> peers_;
};

}  // namespace rdma

#pragma once

#include <string>

#include "rdma/queue_pair.h"

namespace rdma {

struct HandshakeRequest {
  std::string initiator;
  std::string responder;
};

struct HandshakeResponse {
  bool accepted{false};
  std::string detail;
};

class Handshake {
 public:
  static HandshakeRequest Build(const QueuePairConfig& config);
  static HandshakeResponse Validate(const HandshakeRequest& request);
};

}  // namespace rdma

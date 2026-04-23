#include "rdma/handshake.h"

namespace rdma {

HandshakeRequest Handshake::Build(const QueuePairConfig& config) {
  return HandshakeRequest{
      .initiator = config.local_node,
      .responder = config.remote_node,
  };
}

HandshakeResponse Handshake::Validate(const HandshakeRequest& request) {
  if (request.initiator.empty()) {
    return HandshakeResponse{
        .accepted = false,
        .detail = "handshake initiator is empty",
    };
  }

  if (request.responder.empty()) {
    return HandshakeResponse{
        .accepted = false,
        .detail = "handshake responder is empty",
    };
  }

  return HandshakeResponse{
      .accepted = true,
      .detail = "handshake accepted",
  };
}

}  // namespace rdma

#pragma once

#include <cstdint>
#include <string>

#include "rdma/types.h"

namespace rdma {

struct QueuePairConfig {
  std::string local_node;
  std::string remote_node;
  std::uint32_t max_send_wr{128};
  std::uint32_t max_recv_wr{128};
};

class QueuePair {
 public:
  explicit QueuePair(QueuePairConfig config);

  bool TransitionTo(QueuePairState next_state);
  bool BringUp();
  QueuePairState state() const;
  const QueuePairConfig& config() const;
  bool IsReadyToSend() const;
  bool IsReadyToReceive() const;

 private:
  QueuePairConfig config_;
  QueuePairState state_{QueuePairState::kReset};
};

std::string Describe(const QueuePair& qp);

}  // namespace rdma

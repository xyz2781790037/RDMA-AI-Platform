#pragma once

#include <string>

#include "rdma/queue_pair.h"

namespace rdma {

class Connection {
 public:
  explicit Connection(QueuePair queue_pair);

  bool Establish();
  bool Fail();
  bool established() const;
  QueuePair& queue_pair();
  const QueuePair& queue_pair() const;
  std::string channel_id() const;

 private:
  QueuePair queue_pair_;
  bool established_{false};
};

}  // namespace rdma

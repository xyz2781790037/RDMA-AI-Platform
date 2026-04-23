#include "rdma/connection.h"

#include <utility>

namespace rdma {

Connection::Connection(QueuePair queue_pair)
    : queue_pair_(std::move(queue_pair)) {}

bool Connection::Establish() {
  if (established_) {
    return true;
  }
  established_ = queue_pair_.BringUp();
  return established_;
}

bool Connection::Fail() {
  established_ = false;
  return queue_pair_.TransitionTo(QueuePairState::kError);
}

bool Connection::established() const { return established_; }

QueuePair& Connection::queue_pair() { return queue_pair_; }

const QueuePair& Connection::queue_pair() const { return queue_pair_; }

std::string Connection::channel_id() const {
  return queue_pair_.config().local_node + "->" + queue_pair_.config().remote_node;
}

}  // namespace rdma

#include "rdma/queue_pair.h"

#include <utility>

namespace rdma {

QueuePair::QueuePair(QueuePairConfig config) : config_(std::move(config)) {}

bool QueuePair::TransitionTo(QueuePairState next_state) {
  switch (state_) {
    case QueuePairState::kReset:
      if (next_state == QueuePairState::kInit) {
        state_ = next_state;
        return true;
      }
      break;
    case QueuePairState::kInit:
      if (next_state == QueuePairState::kReadyToReceive) {
        state_ = next_state;
        return true;
      }
      break;
    case QueuePairState::kReadyToReceive:
      if (next_state == QueuePairState::kReadyToSend) {
        state_ = next_state;
        return true;
      }
      break;
    case QueuePairState::kReadyToSend:
      if (next_state == QueuePairState::kError) {
        state_ = next_state;
        return true;
      }
      break;
    case QueuePairState::kError:
      break;
  }

  if (next_state == QueuePairState::kError) {
    state_ = next_state;
    return true;
  }

  return false;
}

bool QueuePair::BringUp() {
  if (state_ == QueuePairState::kReset && !TransitionTo(QueuePairState::kInit)) {
    return false;
  }
  if (state_ == QueuePairState::kInit &&
      !TransitionTo(QueuePairState::kReadyToReceive)) {
    return false;
  }
  if (state_ == QueuePairState::kReadyToReceive &&
      !TransitionTo(QueuePairState::kReadyToSend)) {
    return false;
  }
  return state_ == QueuePairState::kReadyToSend;
}

QueuePairState QueuePair::state() const { return state_; }

const QueuePairConfig& QueuePair::config() const { return config_; }

bool QueuePair::IsReadyToSend() const {
  return state_ == QueuePairState::kReadyToSend;
}

bool QueuePair::IsReadyToReceive() const {
  return state_ == QueuePairState::kReadyToReceive ||
         state_ == QueuePairState::kReadyToSend;
}

std::string Describe(const QueuePair& qp) {
  return qp.config().local_node + "->" + qp.config().remote_node + " (" +
         ToString(qp.state()) + ")";
}

}  // namespace rdma

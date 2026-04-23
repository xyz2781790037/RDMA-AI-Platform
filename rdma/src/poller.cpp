#include "rdma/poller.h"

#include <chrono>
#include <utility>

namespace rdma {

Poller::Poller(std::shared_ptr<TransportRdma> transport, std::string node_id,
               CompletionQueue* completions)
    : transport_(std::move(transport)),
      node_id_(std::move(node_id)),
      completions_(completions) {}

Poller::~Poller() { Stop(); }

bool Poller::Start(MessageHandler on_message,
                   CompletionHandler on_completion) {
  if (running_.exchange(true)) {
    return false;
  }

  thread_ = std::thread([this, on_message = std::move(on_message),
                         on_completion = std::move(on_completion)]() mutable {
    using namespace std::chrono_literals;
    while (running_.load()) {
      if (transport_ != nullptr && on_message) {
        std::optional<Message> message = transport_->PollReceive(node_id_, 20ms);
        if (message.has_value()) {
          on_message(*message);
        }
      }

      if (completions_ != nullptr && on_completion) {
        std::optional<CompletionEvent> event = completions_->Poll(5ms);
        if (event.has_value()) {
          on_completion(*event);
        }
      }

      if (transport_ == nullptr && completions_ == nullptr) {
        std::this_thread::sleep_for(20ms);
      }
    }
  });

  return true;
}

void Poller::Stop() {
  running_.store(false);
  if (thread_.joinable()) {
    thread_.join();
  }
}

bool Poller::running() const { return running_.load(); }

}  // namespace rdma

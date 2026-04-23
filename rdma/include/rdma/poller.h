#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "rdma/completion_queue.h"
#include "rdma/transport_rdma.h"

namespace rdma {

using MessageHandler = std::function<void(const Message&)>;
using CompletionHandler = std::function<void(const CompletionEvent&)>;

class Poller {
 public:
  Poller(std::shared_ptr<TransportRdma> transport, std::string node_id,
         CompletionQueue* completions = nullptr);
  ~Poller();

  bool Start(MessageHandler on_message,
             CompletionHandler on_completion = {});
  void Stop();
  bool running() const;

 private:
  std::shared_ptr<TransportRdma> transport_;
  std::string node_id_;
  CompletionQueue* completions_;
  std::atomic<bool> running_{false};
  std::thread thread_;
};

}  // namespace rdma

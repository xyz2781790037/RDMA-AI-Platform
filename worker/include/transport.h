#pragma once

#include <functional>
#include <memory>

#include "task.h"

namespace worker {

using TaskHandler = std::function<void(Task)>;

class Transport {
 public:
  virtual ~Transport() = default;

  virtual bool start() = 0;
  virtual void stop() = 0;
};

std::unique_ptr<Transport> createTcpTransport(int port, TaskHandler handler);

}  // namespace worker

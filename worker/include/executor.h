#pragma once

#include "fake_model.h"
#include "task.h"

namespace worker {

class Executor {
 public:
  TaskResult execute(const Task& task);

 private:
  FakeModel model_;
};

}  // namespace worker

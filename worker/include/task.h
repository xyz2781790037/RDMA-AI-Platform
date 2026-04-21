#pragma once

#include <string>

namespace worker {

struct Task {
  std::string id;
  std::string prompt;
};

struct TaskResult {
  std::string task_id;
  std::string output;
  bool success = false;
};

}  // namespace worker

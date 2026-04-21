#include "executor.h"

#include <exception>

namespace worker {

TaskResult Executor::execute(const Task& task) {
  TaskResult result;
  result.task_id = task.id;

  try {
    result.output = model_.infer(task.prompt);
    result.success = true;
  } catch (const std::exception& ex) {
    result.output = ex.what();
    result.success = false;
  } catch (...) {
    result.output = "unknown executor error";
    result.success = false;
  }

  return result;
}

}  // namespace worker

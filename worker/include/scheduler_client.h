#pragma once

#include <string>
#include <vector>

#include "task.h"

namespace worker {

class SchedulerClient {
 public:
  SchedulerClient() = default;
  SchedulerClient(std::string scheduler_addr, std::string worker_id);

  void configure(std::string scheduler_addr, std::string worker_id);
  bool registerWorker(const std::string& worker_address, int capacity);
  bool sendHeartbeat(int running_tasks, const std::vector<std::string>& active_task_ids);
  bool reportResult(const TaskResult& result);

 private:
  int postJSON(const std::string& path, const std::string& body) const;

  std::string scheduler_addr_;
  std::string worker_id_;
};

}  // namespace worker

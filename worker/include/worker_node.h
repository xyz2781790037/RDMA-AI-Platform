#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "executor.h"
#include "scheduler_client.h"
#include "task_queue.h"
#include "thread_pool.h"
#include "timer.h"
#include "transport.h"

namespace worker {

struct WorkerConfig {
  std::string worker_id = "worker-1";
  int listen_port = 9001;
  std::string scheduler_addr = "127.0.0.1:8080";
  int heartbeat_interval = 5;
  int threads = 4;
};

class WorkerNode {
 public:
  explicit WorkerNode(std::string config_path = "config/worker.yaml");
  ~WorkerNode();

  bool start();
  void stop();
  void run();

 private:
  bool loadConfig();
  bool enqueueTask(Task task);
  std::vector<std::string> snapshotActiveTaskIDs() const;
  int runningTaskCount() const;
  void finishTask(const std::string& task_id);
  std::string workerAddress() const;
  void registerToScheduler();
  void startHeartbeat();
  void startTaskServer();
  void startConsumers();

  std::string config_path_;
  WorkerConfig config_;
  std::atomic<bool> running_{false};
  TaskQueue task_queue_;
  SchedulerClient scheduler_client_;
  std::unique_ptr<Transport> transport_;
  std::unique_ptr<ThreadPool> thread_pool_;
  Timer heartbeat_timer_;
  Executor executor_;
  std::mutex state_mutex_;
  std::condition_variable state_condition_;
  mutable std::mutex active_tasks_mutex_;
  std::unordered_set<std::string> active_task_ids_;
};

}  // namespace worker

#include "worker_node.h"

#include <chrono>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "logger.h"

namespace worker {
namespace {

std::string trim(const std::string& value) {
  const std::size_t begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }

  const std::size_t end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

bool parseInt(const std::string& text, int& value) {
  try {
    std::size_t index = 0;
    const int parsed = std::stoi(text, &index);
    if (index != text.size()) {
      return false;
    }
    value = parsed;
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace

WorkerNode::WorkerNode(std::string config_path) : config_path_(std::move(config_path)) {}

WorkerNode::~WorkerNode() {
  stop();
}

bool WorkerNode::start() {
  if (running_.load()) {
    return true;
  }

  INFO("worker starting");

  if (!loadConfig()) {
    return false;
  }

  scheduler_client_.configure(config_.scheduler_addr, config_.worker_id);
  running_.store(true);

  try {
    startTaskServer();
    startConsumers();
    registerToScheduler();
    startHeartbeat();
  } catch (const std::exception& ex) {
    ERROR("worker start failed: " + std::string(ex.what()));
    stop();
    return false;
  }

  INFO("worker started");
  return true;
}

void WorkerNode::stop() {
  const bool was_running = running_.exchange(false);

  heartbeat_timer_.stop();
  if (was_running) {
    scheduler_client_.sendHeartbeat(0, {});
  }

  if (transport_) {
    transport_->stop();
  }

  task_queue_.stop();

  if (thread_pool_) {
    thread_pool_->stop();
  }

  state_condition_.notify_all();

  if (was_running) {
    INFO("worker stopped");
  }
}

void WorkerNode::run() {
  std::unique_lock<std::mutex> lock(state_mutex_);
  state_condition_.wait(lock, [this] { return !running_.load(); });
}

bool WorkerNode::loadConfig() {
  std::ifstream config_stream(config_path_);
  if (!config_stream.is_open()) {
    ERROR("open config failed: " + config_path_);
    return false;
  }

  WorkerConfig loaded = config_;
  std::string line;

  while (std::getline(config_stream, line)) {
    const std::size_t comment_pos = line.find('#');
    if (comment_pos != std::string::npos) {
      line = line.substr(0, comment_pos);
    }

    line = trim(line);
    if (line.empty()) {
      continue;
    }

    const std::size_t separator = line.find(':');
    if (separator == std::string::npos) {
      ERROR("invalid config line: " + line);
      return false;
    }

    const std::string key = trim(line.substr(0, separator));
    const std::string value = trim(line.substr(separator + 1));

    if (key == "worker_id") {
      loaded.worker_id = value;
    } else if (key == "listen_port") {
      if (!parseInt(value, loaded.listen_port)) {
        ERROR("invalid listen_port: " + value);
        return false;
      }
    } else if (key == "scheduler_addr") {
      loaded.scheduler_addr = value;
    } else if (key == "heartbeat_interval") {
      if (!parseInt(value, loaded.heartbeat_interval)) {
        ERROR("invalid heartbeat_interval: " + value);
        return false;
      }
    } else if (key == "threads") {
      if (!parseInt(value, loaded.threads)) {
        ERROR("invalid threads: " + value);
        return false;
      }
    }
  }

  if (loaded.worker_id.empty()) {
    ERROR("config worker_id is empty");
    return false;
  }

  if (loaded.listen_port <= 0) {
    ERROR("config listen_port must be greater than zero");
    return false;
  }

  if (loaded.scheduler_addr.empty()) {
    ERROR("config scheduler_addr is empty");
    return false;
  }

  if (loaded.heartbeat_interval <= 0) {
    ERROR("config heartbeat_interval must be greater than zero");
    return false;
  }

  if (loaded.threads <= 0) {
    ERROR("config threads must be greater than zero");
    return false;
  }

  config_ = std::move(loaded);
  return true;
}

bool WorkerNode::enqueueTask(Task task) {
  std::string task_id = task.id;
  {
    std::lock_guard<std::mutex> lock(active_tasks_mutex_);
    const auto [_, inserted] = active_task_ids_.insert(task_id);
    if (!inserted) {
      INFO("skip duplicate task " + task_id);
      return true;
    }
  }

  if (!task_queue_.push(std::move(task))) {
    finishTask(task_id);
    return false;
  }

  return true;
}

std::vector<std::string> WorkerNode::snapshotActiveTaskIDs() const {
  std::lock_guard<std::mutex> lock(active_tasks_mutex_);
  return std::vector<std::string>(active_task_ids_.begin(), active_task_ids_.end());
}

int WorkerNode::runningTaskCount() const {
  std::lock_guard<std::mutex> lock(active_tasks_mutex_);
  return static_cast<int>(active_task_ids_.size());
}

void WorkerNode::finishTask(const std::string& task_id) {
  std::lock_guard<std::mutex> lock(active_tasks_mutex_);
  active_task_ids_.erase(task_id);
}

std::string WorkerNode::workerAddress() const {
  return "127.0.0.1:" + std::to_string(config_.listen_port);
}

void WorkerNode::registerToScheduler() {
  if (!scheduler_client_.registerWorker(workerAddress(), config_.threads)) {
    throw std::runtime_error("register to scheduler failed");
  }

  INFO("register success");
}

void WorkerNode::startHeartbeat() {
  const bool started = heartbeat_timer_.start(
      std::chrono::seconds(config_.heartbeat_interval),
      [this]() {
        if (running_.load()) {
          scheduler_client_.sendHeartbeat(runningTaskCount(), snapshotActiveTaskIDs());
        }
      });

  if (!started) {
    throw std::runtime_error("start heartbeat failed");
  }

  INFO("heartbeat thread started");
}

void WorkerNode::startTaskServer() {
  transport_ = createTcpTransport(config_.listen_port, [this](Task task) {
    if (!running_.load()) {
      return;
    }

    INFO("recv task " + task.id);
    if (!enqueueTask(std::move(task))) {
      ERROR("enqueue task failed");
    }
  });

  if (!transport_ || !transport_->start()) {
    throw std::runtime_error("start task server failed");
  }

  INFO("task server listen " + std::to_string(config_.listen_port));
}

void WorkerNode::startConsumers() {
  thread_pool_ = std::make_unique<ThreadPool>(static_cast<std::size_t>(config_.threads));
  if (!thread_pool_->start()) {
    throw std::runtime_error("start consumer thread pool failed");
  }

  for (int i = 0; i < config_.threads; ++i) {
    const bool queued = thread_pool_->enqueue([this]() {
      while (running_.load()) {
        Task task;
        if (!task_queue_.waitPop(task)) {
          break;
        }

        INFO("execute task " + task.id);
        TaskResult result = executor_.execute(task);
        INFO("finish task " + task.id + " => " + result.output);

        if (!scheduler_client_.reportResult(result)) {
          ERROR("report result failed for task " + task.id);
        }
        finishTask(task.id);
      }
    });

    if (!queued) {
      throw std::runtime_error("submit consumer task failed");
    }
  }

  INFO("consumer threads started");
}

}  // namespace worker

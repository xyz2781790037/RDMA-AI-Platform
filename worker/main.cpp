#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

#include "logger.h"
#include "worker_node.h"

namespace {

std::atomic<bool> g_stop_requested{false};
std::atomic<bool> g_exit_requested{false};

void handleSignal(int) {
  g_stop_requested.store(true);
}

}  // namespace

int main() {
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  worker::WorkerNode node;
  if (!node.start()) {
    return 1;
  }

  std::thread signal_watcher([&node]() {
    while (!g_stop_requested.load() && !g_exit_requested.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (g_stop_requested.load()) {
      INFO("stop signal received");
      node.stop();
    }
  });

  node.run();

  g_exit_requested.store(true);
  if (signal_watcher.joinable()) {
    signal_watcher.join();
  }

  return 0;
}

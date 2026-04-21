#include "fake_model.h"

#include <chrono>
#include <random>
#include <thread>

namespace worker {

std::string FakeModel::infer(const std::string& prompt) const {
  thread_local std::mt19937 generator(std::random_device{}());
  std::uniform_int_distribution<int> distribution(100, 300);

  std::this_thread::sleep_for(std::chrono::milliseconds(distribution(generator)));
  return "processed: " + prompt;
}

}  // namespace worker

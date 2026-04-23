#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_set>

#include "rdma/config.h"
#include "rdma/memory_region.h"

namespace rdma {

class BufferPool {
 public:
  BufferPool(BufferPoolConfig config, MemoryRegistry& registry);

  MemoryRegion Acquire();
  void Release(const std::string& key);
  std::size_t Available() const;
  const BufferPoolConfig& config() const;

 private:
  BufferPoolConfig config_;
  MemoryRegistry* registry_;
  mutable std::mutex mutex_;
  std::deque<std::string> free_keys_;
  std::unordered_set<std::string> known_keys_;
};

}  // namespace rdma

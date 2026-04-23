#include "rdma/buffer_pool.h"

#include <algorithm>
#include <utility>

namespace rdma {

BufferPool::BufferPool(BufferPoolConfig config, MemoryRegistry& registry)
    : config_(std::move(config)), registry_(&registry) {
  for (std::size_t i = 0; i < config_.buffers; ++i) {
    MemoryRegion region = registry_->Register(config_.buffer_size);
    free_keys_.push_back(region.key());
    known_keys_.insert(region.key());
  }
}

MemoryRegion BufferPool::Acquire() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (free_keys_.empty()) {
    MemoryRegion region = registry_->Register(config_.buffer_size);
    known_keys_.insert(region.key());
    return region;
  }

  std::string key = std::move(free_keys_.front());
  free_keys_.pop_front();
  MemoryRegion region = registry_->Lookup(key);
  if (region.storage() != nullptr && region.storage()->size() != config_.buffer_size) {
    region.storage()->resize(config_.buffer_size);
  }
  return region;
}

void BufferPool::Release(const std::string& key) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!known_keys_.contains(key)) {
    return;
  }

  if (std::find(free_keys_.begin(), free_keys_.end(), key) != free_keys_.end()) {
    return;
  }

  MemoryRegion region = registry_->Lookup(key);
  if (region.storage() != nullptr) {
    region.storage()->resize(config_.buffer_size);
  }
  free_keys_.push_back(key);
}

std::size_t BufferPool::Available() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return free_keys_.size();
}

const BufferPoolConfig& BufferPool::config() const { return config_; }

}  // namespace rdma

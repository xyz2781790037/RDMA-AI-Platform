#include "rdma/memory_region.h"

#include <stdexcept>
#include <utility>

namespace rdma {

MemoryRegion::MemoryRegion(std::string key, std::uint32_t local_key,
                           std::uint32_t remote_key, ByteBufferPtr storage)
    : key_(std::move(key)),
      local_key_(local_key),
      remote_key_(remote_key),
      storage_(std::move(storage)) {}

const std::string& MemoryRegion::key() const { return key_; }

std::uint32_t MemoryRegion::local_key() const { return local_key_; }

std::uint32_t MemoryRegion::remote_key() const { return remote_key_; }

std::size_t MemoryRegion::size() const {
  return storage_ == nullptr ? 0U : storage_->size();
}

const ByteBufferPtr& MemoryRegion::storage() const { return storage_; }

MemoryRegion MemoryRegistry::Register(std::size_t bytes) {
  return Register(ByteBuffer(bytes));
}

MemoryRegion MemoryRegistry::Register(const ByteBuffer& bytes) {
  return Register(ByteBuffer(bytes));
}

MemoryRegion MemoryRegistry::Register(ByteBuffer&& bytes) {
  std::lock_guard<std::mutex> lock(mutex_);

  const std::string key = NextKey();
  auto storage = std::make_shared<ByteBuffer>(std::move(bytes));
  MemoryRegion region(key, NextAccessKey(), NextAccessKey(), storage);
  regions_.emplace(key, region);
  return region;
}

MemoryRegion MemoryRegistry::Lookup(const std::string& key) const {
  std::lock_guard<std::mutex> lock(mutex_);

  const auto it = regions_.find(key);
  if (it == regions_.end()) {
    throw std::runtime_error("memory region not found: " + key);
  }

  return it->second;
}

void MemoryRegistry::Release(const std::string& key) {
  std::lock_guard<std::mutex> lock(mutex_);
  regions_.erase(key);
}

std::size_t MemoryRegistry::Size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return regions_.size();
}

std::string MemoryRegistry::NextKey() {
  ++next_region_id_;
  return "mr-" + std::to_string(next_region_id_);
}

std::uint32_t MemoryRegistry::NextAccessKey() { return next_access_key_++; }

}  // namespace rdma

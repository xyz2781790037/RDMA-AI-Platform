#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "rdma/types.h"

namespace rdma {

class MemoryRegion {
 public:
  MemoryRegion() = default;
  MemoryRegion(std::string key, std::uint32_t local_key,
               std::uint32_t remote_key, ByteBufferPtr storage);

  const std::string& key() const;
  std::uint32_t local_key() const;
  std::uint32_t remote_key() const;
  std::size_t size() const;
  const ByteBufferPtr& storage() const;

 private:
  std::string key_;
  std::uint32_t local_key_{0};
  std::uint32_t remote_key_{0};
  ByteBufferPtr storage_;
};

class MemoryRegistry {
 public:
  MemoryRegion Register(std::size_t bytes);
  MemoryRegion Register(const ByteBuffer& bytes);
  MemoryRegion Register(ByteBuffer&& bytes);
  MemoryRegion Lookup(const std::string& key) const;
  void Release(const std::string& key);
  std::size_t Size() const;

 private:
  std::string NextKey();
  std::uint32_t NextAccessKey();

  mutable std::mutex mutex_;
  std::uint64_t next_region_id_{0};
  std::uint32_t next_access_key_{1024};
  std::unordered_map<std::string, MemoryRegion> regions_;
};

}  // namespace rdma

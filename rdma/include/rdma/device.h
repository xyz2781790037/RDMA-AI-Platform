#pragma once

#include <string>

#include "rdma/config.h"

namespace rdma {

class Device {
 public:
  explicit Device(DeviceConfig config = {});

  const DeviceConfig& config() const;
  std::string Name() const;
  std::uint16_t Port() const;
  bool IsLoopback() const;

 private:
  DeviceConfig config_;
};

}  // namespace rdma

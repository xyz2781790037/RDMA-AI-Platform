#pragma once

#include <memory>

#include "rdma/device.h"

namespace rdma {

class Context {
 public:
  explicit Context(std::shared_ptr<Device> device);

  const Device& device() const;
  std::shared_ptr<Device> device_handle() const;

 private:
  std::shared_ptr<Device> device_;
};

std::shared_ptr<Context> MakeDefaultContext(const DeviceConfig& config = {});

}  // namespace rdma

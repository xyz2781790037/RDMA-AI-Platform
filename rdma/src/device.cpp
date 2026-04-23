#include "rdma/device.h"

#include <utility>

namespace rdma {

Device::Device(DeviceConfig config) : config_(std::move(config)) {}

const DeviceConfig& Device::config() const { return config_; }

std::string Device::Name() const { return config_.name; }

std::uint16_t Device::Port() const { return config_.port; }

bool Device::IsLoopback() const { return config_.name == "loopback0"; }

}  // namespace rdma

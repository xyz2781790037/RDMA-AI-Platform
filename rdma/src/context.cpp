#include "rdma/context.h"

#include <utility>

namespace rdma {

Context::Context(std::shared_ptr<Device> device)
    : device_(device == nullptr ? std::make_shared<Device>() : std::move(device)) {}

const Device& Context::device() const { return *device_; }

std::shared_ptr<Device> Context::device_handle() const { return device_; }

std::shared_ptr<Context> MakeDefaultContext(const DeviceConfig& config) {
  return std::make_shared<Context>(std::make_shared<Device>(config));
}

}  // namespace rdma

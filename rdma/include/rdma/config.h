#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace rdma {

struct DeviceConfig {
  std::string name{"loopback0"};
  std::uint16_t port{1};
};

struct CompletionQueueConfig {
  std::size_t depth{1024};
};

struct BufferPoolConfig {
  std::size_t buffers{8};
  std::size_t buffer_size{64 * 1024};
};

struct TransportConfig {
  std::string backend_name{"in-memory-rdma"};
  std::size_t mailbox_capacity{4096};
};

struct ServerConfig {
  std::string node_id{"rdma-server"};
  std::string listen_address{"loopback"};
};

struct ClientConfig {
  std::string node_id{"rdma-client"};
  std::string server_node{"rdma-server"};
};

struct WorkerSessionConfig {
  std::string node_id{"worker"};
  BufferPoolConfig buffer_pool{};
  CompletionQueueConfig completion_queue{};
};

struct RdmaConfig {
  DeviceConfig device{};
  BufferPoolConfig buffer_pool{};
  CompletionQueueConfig completion_queue{};
  TransportConfig transport{};
};

RdmaConfig DefaultRdmaConfig();
std::string Summarize(const RdmaConfig& config);

}  // namespace rdma

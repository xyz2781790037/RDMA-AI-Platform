#include "rdma/config.h"

#include <sstream>

namespace rdma {

RdmaConfig DefaultRdmaConfig() { return RdmaConfig{}; }

std::string Summarize(const RdmaConfig& config) {
  std::ostringstream stream;
  stream << "device=" << config.device.name << ":" << config.device.port
         << ", cq_depth=" << config.completion_queue.depth
         << ", buffers=" << config.buffer_pool.buffers
         << ", buffer_size=" << config.buffer_pool.buffer_size
         << ", backend=" << config.transport.backend_name;
  return stream.str();
}

}  // namespace rdma

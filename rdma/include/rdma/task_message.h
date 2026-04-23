#pragma once

#include <map>
#include <sstream>
#include <string>

#include "rdma/types.h"

namespace rdma {

struct TaskMessage {
  std::string task_id;
  std::string kind;
  std::string source_node;
  std::string target_node;
  std::string description;
  std::map<std::string, std::string> metadata;

  ByteBuffer Serialize() const {
    std::ostringstream stream;
    stream << "task_id=" << task_id << "\n";
    stream << "kind=" << kind << "\n";
    stream << "source_node=" << source_node << "\n";
    stream << "target_node=" << target_node << "\n";
    stream << "description=" << description << "\n";
    for (const auto& [key, value] : metadata) {
      stream << "meta:" << key << "=" << value << "\n";
    }
    return BytesFromString(stream.str());
  }

  static TaskMessage Deserialize(const ByteBuffer& payload) {
    TaskMessage message;
    std::istringstream stream(StringFromBytes(payload));
    std::string line;

    while (std::getline(stream, line)) {
      if (line.empty()) {
        continue;
      }

      if (line.rfind("meta:", 0) == 0) {
        const std::string pair = line.substr(5);
        const std::size_t pos = pair.find('=');
        if (pos != std::string::npos) {
          message.metadata[pair.substr(0, pos)] = pair.substr(pos + 1);
        }
        continue;
      }

      const std::size_t pos = line.find('=');
      if (pos == std::string::npos) {
        continue;
      }

      const std::string key = line.substr(0, pos);
      const std::string value = line.substr(pos + 1);

      if (key == "task_id") {
        message.task_id = value;
      } else if (key == "kind") {
        message.kind = value;
      } else if (key == "source_node") {
        message.source_node = value;
      } else if (key == "target_node") {
        message.target_node = value;
      } else if (key == "description") {
        message.description = value;
      }
    }

    return message;
  }

  static TaskMessage Deserialize(const ByteBufferPtr& payload) {
    return payload == nullptr ? TaskMessage{} : Deserialize(*payload);
  }
};

}  // namespace rdma

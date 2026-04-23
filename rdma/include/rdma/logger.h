#pragma once

#include <mutex>
#include <string_view>

#include "rdma/types.h"

namespace rdma {

class Logger {
 public:
  static Logger& Instance();

  void SetLevel(LogLevel level);
  LogLevel level() const;
  void Log(LogLevel level, std::string_view message);

 private:
  Logger() = default;

  mutable std::mutex mutex_;
  LogLevel level_{LogLevel::kInfo};
};

void SetLogLevel(LogLevel level);
void Log(LogLevel level, std::string_view message);

}  // namespace rdma

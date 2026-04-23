#include "rdma/logger.h"

#include <iostream>

namespace rdma {

Logger& Logger::Instance() {
  static Logger logger;
  return logger;
}

void Logger::SetLevel(LogLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);
  level_ = level;
}

LogLevel Logger::level() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return level_;
}

void Logger::Log(LogLevel level, std::string_view message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (static_cast<int>(level) < static_cast<int>(level_)) {
    return;
  }

  std::clog << "[rdma][" << ToString(level) << "] " << message << '\n';
}

void SetLogLevel(LogLevel level) { Logger::Instance().SetLevel(level); }

void Log(LogLevel level, std::string_view message) {
  Logger::Instance().Log(level, message);
}

}  // namespace rdma

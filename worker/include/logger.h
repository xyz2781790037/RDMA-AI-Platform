#pragma once

#include <mutex>
#include <string>

namespace worker {

enum class LogLevel {
  Info,
  Error,
};

class Logger {
 public:
  static void log(LogLevel level, const std::string& message);

 private:
  static std::mutex mutex_;
};

}  // namespace worker

#define INFO(message) ::worker::Logger::log(::worker::LogLevel::Info, (message))
#define ERROR(message) ::worker::Logger::log(::worker::LogLevel::Error, (message))

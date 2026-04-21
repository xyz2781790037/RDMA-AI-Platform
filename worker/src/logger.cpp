#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace worker {

std::mutex Logger::mutex_;

void Logger::log(LogLevel level, const std::string& message) {
  const auto now = std::chrono::system_clock::now();
  const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

  std::tm local_tm {};
  localtime_r(&now_time, &local_tm);

  std::ostringstream stream;
  stream << "[" << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << "]";
  stream << "[" << (level == LogLevel::Info ? "INFO" : "ERROR") << "] " << message;

  std::lock_guard<std::mutex> lock(mutex_);
  std::ostream& output = level == LogLevel::Info ? std::cout : std::cerr;
  output << stream.str() << std::endl;
}

}  // namespace worker

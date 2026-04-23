#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace rdma {

using ByteBuffer = std::vector<std::byte>;
using ByteBufferPtr = std::shared_ptr<ByteBuffer>;
using WorkRequestId = std::uint64_t;
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

enum class CompletionStatus {
  kSuccess,
  kFailed,
  kTimeout,
};

enum class QueuePairState {
  kReset,
  kInit,
  kReadyToReceive,
  kReadyToSend,
  kError,
};

enum class OperationType {
  kSend,
  kWrite,
  kResult,
};

enum class LogLevel {
  kDebug,
  kInfo,
  kWarn,
  kError,
};

inline std::string ToString(CompletionStatus status) {
  switch (status) {
    case CompletionStatus::kSuccess:
      return "success";
    case CompletionStatus::kFailed:
      return "failed";
    case CompletionStatus::kTimeout:
      return "timeout";
  }
  return "unknown";
}

inline std::string ToString(QueuePairState state) {
  switch (state) {
    case QueuePairState::kReset:
      return "reset";
    case QueuePairState::kInit:
      return "init";
    case QueuePairState::kReadyToReceive:
      return "ready_to_receive";
    case QueuePairState::kReadyToSend:
      return "ready_to_send";
    case QueuePairState::kError:
      return "error";
  }
  return "unknown";
}

inline std::string ToString(OperationType type) {
  switch (type) {
    case OperationType::kSend:
      return "send";
    case OperationType::kWrite:
      return "write";
    case OperationType::kResult:
      return "result";
  }
  return "unknown";
}

inline std::string ToString(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return "debug";
    case LogLevel::kInfo:
      return "info";
    case LogLevel::kWarn:
      return "warn";
    case LogLevel::kError:
      return "error";
  }
  return "unknown";
}

inline ByteBuffer BytesFromString(std::string_view text) {
  ByteBuffer bytes;
  bytes.reserve(text.size());
  for (char ch : text) {
    bytes.push_back(std::byte{static_cast<unsigned char>(ch)});
  }
  return bytes;
}

inline std::string StringFromBytes(const ByteBuffer& bytes) {
  std::string text;
  text.reserve(bytes.size());
  for (std::byte value : bytes) {
    text.push_back(static_cast<char>(std::to_integer<unsigned char>(value)));
  }
  return text;
}

inline std::string StringFromBytes(const ByteBufferPtr& bytes) {
  return bytes == nullptr ? std::string{} : StringFromBytes(*bytes);
}

}  // namespace rdma

#include "scheduler_client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <sstream>
#include <string_view>
#include <utility>

#include "logger.h"

namespace worker {
namespace {

std::string escapeJSONString(std::string_view input) {
  std::string output;
  output.reserve(input.size());

  for (const char ch : input) {
    switch (ch) {
      case '\\':
        output += "\\\\";
        break;
      case '"':
        output += "\\\"";
        break;
      case '\n':
        output += "\\n";
        break;
      case '\r':
        output += "\\r";
        break;
      case '\t':
        output += "\\t";
        break;
      default:
        output.push_back(ch);
        break;
    }
  }

  return output;
}

bool splitHostPort(const std::string& address, std::string& host, std::string& port) {
  std::string normalized = address;
  constexpr std::string_view http_prefix = "http://";
  if (normalized.rfind(http_prefix.data(), 0) == 0) {
    normalized.erase(0, http_prefix.size());
  }

  const std::size_t slash = normalized.find('/');
  if (slash != std::string::npos) {
    normalized = normalized.substr(0, slash);
  }

  const std::size_t separator = normalized.rfind(':');
  if (separator == std::string::npos || separator == 0 || separator == normalized.size() - 1) {
    return false;
  }

  host = normalized.substr(0, separator);
  port = normalized.substr(separator + 1);
  return true;
}

}  // namespace

SchedulerClient::SchedulerClient(std::string scheduler_addr, std::string worker_id)
    : scheduler_addr_(std::move(scheduler_addr)), worker_id_(std::move(worker_id)) {}

void SchedulerClient::configure(std::string scheduler_addr, std::string worker_id) {
  scheduler_addr_ = std::move(scheduler_addr);
  worker_id_ = std::move(worker_id);
}

bool SchedulerClient::registerWorker(const std::string& worker_address, int capacity) {
  if (worker_id_.empty()) {
    ERROR("scheduler register failed: worker id is empty");
    return false;
  }

  std::ostringstream body;
  body << "{\"id\":\"" << escapeJSONString(worker_id_) << "\","
       << "\"address\":\"" << escapeJSONString(worker_address) << "\","
       << "\"capacity\":" << capacity << "}";

  const int status = postJSON("/workers/register", body.str());
  if (status != 201) {
    ERROR("scheduler register failed with status " + std::to_string(status));
    return false;
  }

  INFO("[Scheduler] register worker " + worker_id_);
  return true;
}

bool SchedulerClient::sendHeartbeat(int running_tasks, const std::vector<std::string>& active_task_ids) {
  if (worker_id_.empty()) {
    ERROR("scheduler heartbeat failed: worker id is empty");
    return false;
  }

  std::ostringstream body;
  body << "{\"id\":\"" << escapeJSONString(worker_id_) << "\","
       << "\"running_tasks\":" << running_tasks << ","
       << "\"status\":\"" << (running_tasks > 0 ? "busy" : "ready") << "\"";
  if (!active_task_ids.empty()) {
    body << ",\"active_task_ids\":[";
    for (std::size_t i = 0; i < active_task_ids.size(); ++i) {
      if (i > 0) {
        body << ",";
      }
      body << "\"" << escapeJSONString(active_task_ids[i]) << "\"";
    }
    body << "]";
  }
  body << "}";

  const int status = postJSON("/workers/heartbeat", body.str());
  if (status != 200) {
    ERROR("scheduler heartbeat failed with status " + std::to_string(status));
    return false;
  }

  INFO("[Scheduler] heartbeat");
  return true;
}

bool SchedulerClient::reportResult(const TaskResult& result) {
  if (worker_id_.empty()) {
    ERROR("scheduler result report failed: worker id is empty");
    return false;
  }

  std::ostringstream body;
  body << "{\"success\":" << (result.success ? "true" : "false") << ","
       << "\"message\":\"" << escapeJSONString(result.output) << "\"}";

  const int status = postJSON("/tasks/" + result.task_id + "/complete", body.str());
  if (status != 200) {
    ERROR("scheduler result report failed with status " + std::to_string(status));
    return false;
  }

  INFO("[Scheduler] result " + result.task_id + " " + (result.success ? "success" : "failure"));
  return true;
}

int SchedulerClient::postJSON(const std::string& path, const std::string& body) const {
  std::string host;
  std::string port;
  if (!splitHostPort(scheduler_addr_, host, port)) {
    ERROR("invalid scheduler address: " + scheduler_addr_);
    return -1;
  }

  addrinfo hints {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* results = nullptr;
  if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &results) != 0) {
    ERROR("resolve scheduler address failed: " + scheduler_addr_);
    return -1;
  }

  int socket_fd = -1;
  for (addrinfo* current = results; current != nullptr; current = current->ai_next) {
    socket_fd = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
    if (socket_fd < 0) {
      continue;
    }

    if (::connect(socket_fd, current->ai_addr, current->ai_addrlen) == 0) {
      break;
    }

    ::close(socket_fd);
    socket_fd = -1;
  }

  ::freeaddrinfo(results);

  if (socket_fd < 0) {
    ERROR("connect scheduler failed: " + scheduler_addr_ + " err=" + std::string(std::strerror(errno)));
    return -1;
  }

  std::ostringstream request;
  request << "POST " << path << " HTTP/1.1\r\n"
          << "Host: " << host << ":" << port << "\r\n"
          << "Content-Type: application/json\r\n"
          << "Connection: close\r\n"
          << "Content-Length: " << body.size() << "\r\n\r\n"
          << body;

  const std::string serialized = request.str();
  std::size_t sent = 0;
  while (sent < serialized.size()) {
    const ssize_t written = ::send(
        socket_fd,
        serialized.data() + sent,
        serialized.size() - sent,
        0);
    if (written < 0) {
      ERROR("send scheduler request failed: " + std::string(std::strerror(errno)));
      ::close(socket_fd);
      return -1;
    }
    sent += static_cast<std::size_t>(written);
  }

  std::string response;
  char buffer[1024];
  while (true) {
    const ssize_t received = ::recv(socket_fd, buffer, sizeof(buffer), 0);
    if (received < 0) {
      if (errno == EINTR) {
        continue;
      }
      ERROR("recv scheduler response failed: " + std::string(std::strerror(errno)));
      ::close(socket_fd);
      return -1;
    }
    if (received == 0) {
      break;
    }
    response.append(buffer, static_cast<std::size_t>(received));
  }

  ::close(socket_fd);

  const std::size_t line_end = response.find("\r\n");
  if (line_end == std::string::npos) {
    ERROR("invalid scheduler response");
    return -1;
  }

  std::istringstream status_line(response.substr(0, line_end));
  std::string http_version;
  int status_code = -1;
  status_line >> http_version >> status_code;
  return status_code;
}

}  // namespace worker

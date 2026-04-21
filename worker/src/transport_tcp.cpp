#include "transport.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <exception>
#include <string>
#include <thread>
#include <utility>

#include "logger.h"

namespace worker {
namespace {

class TcpTransport final : public Transport {
 public:
  TcpTransport(int port, TaskHandler handler)
      : port_(port), handler_(std::move(handler)) {}

  ~TcpTransport() override {
    stop();
  }

  bool start() override {
    if (running_.load()) {
      return true;
    }

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
      ERROR("create socket failed: " + std::string(std::strerror(errno)));
      return false;
    }

    int reuse = 1;
    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
      ERROR("setsockopt failed: " + std::string(std::strerror(errno)));
      closeListenSocket();
      return false;
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<uint16_t>(port_));

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
      ERROR("bind failed: " + std::string(std::strerror(errno)));
      closeListenSocket();
      return false;
    }

    if (::listen(listen_fd_, 16) < 0) {
      ERROR("listen failed: " + std::string(std::strerror(errno)));
      closeListenSocket();
      return false;
    }

    running_.store(true);
    accept_thread_ = std::thread(&TcpTransport::acceptLoop, this);
    return true;
  }

  void stop() override {
    const bool was_running = running_.exchange(false);

    if (was_running && listen_fd_ >= 0) {
      ::shutdown(listen_fd_, SHUT_RDWR);
      closeListenSocket();
    }

    if (accept_thread_.joinable()) {
      accept_thread_.join();
    }

    if (!was_running) {
      closeListenSocket();
    }
  }

 private:
  void acceptLoop() {
    while (running_.load()) {
      sockaddr_in client_address {};
      socklen_t client_length = sizeof(client_address);

      const int client_fd =
          ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);
      if (client_fd < 0) {
        if (!running_.load()) {
          break;
        }
        if (errno == EINTR) {
          continue;
        }
        ERROR("accept failed: " + std::string(std::strerror(errno)));
        continue;
      }

      timeval timeout {};
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      ::setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

      handleClient(client_fd);
      ::close(client_fd);
    }
  }

  void handleClient(int client_fd) {
    std::string payload;
    char buffer[1024];

    while (true) {
      const ssize_t received = ::recv(client_fd, buffer, sizeof(buffer), 0);
      if (received < 0) {
        if (errno == EINTR) {
          continue;
        }
        if ((errno == EAGAIN || errno == EWOULDBLOCK) && running_.load()) {
          continue;
        }
        if ((errno == EAGAIN || errno == EWOULDBLOCK) && !running_.load()) {
          return;
        }
        ERROR("recv failed: " + std::string(std::strerror(errno)));
        return;
      }

      if (received == 0) {
        break;
      }

      payload.append(buffer, static_cast<std::size_t>(received));
      const std::size_t newline_pos = payload.find('\n');
      if (newline_pos != std::string::npos) {
        payload = payload.substr(0, newline_pos);
        break;
      }
    }

    if (!payload.empty() && payload.back() == '\r') {
      payload.pop_back();
    }

    if (payload.empty()) {
      return;
    }

    const std::size_t separator = payload.find('|');
    if (separator == std::string::npos) {
      ERROR("invalid task payload: " + payload);
      return;
    }

    Task task;
    task.id = payload.substr(0, separator);
    task.prompt = payload.substr(separator + 1);

    if (task.id.empty()) {
      ERROR("invalid task payload: empty task id");
      return;
    }

    try {
      handler_(std::move(task));
    } catch (const std::exception& ex) {
      ERROR("task dispatch failed: " + std::string(ex.what()));
    } catch (...) {
      ERROR("task dispatch failed: unknown exception");
    }
  }

  void closeListenSocket() {
    if (listen_fd_ >= 0) {
      ::close(listen_fd_);
      listen_fd_ = -1;
    }
  }

  int port_;
  TaskHandler handler_;
  int listen_fd_ = -1;
  std::atomic<bool> running_{false};
  std::thread accept_thread_;
};

}  // namespace

std::unique_ptr<Transport> createTcpTransport(int port, TaskHandler handler) {
  return std::make_unique<TcpTransport>(port, std::move(handler));
}

}  // namespace worker

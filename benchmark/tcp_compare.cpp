#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

namespace {

bool SendAll(int fd, const char* data, std::size_t bytes) {
  std::size_t sent = 0;
  while (sent < bytes) {
    const ssize_t rc = send(fd, data + sent, bytes - sent, 0);
    if (rc <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(rc);
  }
  return true;
}

bool RecvAll(int fd, char* data, std::size_t bytes) {
  std::size_t received = 0;
  while (received < bytes) {
    const ssize_t rc = recv(fd, data + received, bytes - received, 0);
    if (rc <= 0) {
      return false;
    }
    received += static_cast<std::size_t>(rc);
  }
  return true;
}

}  // namespace

int main() {
  constexpr int kRounds = 2000;
  constexpr std::size_t kPayloadBytes = 4 * 1024;
  constexpr std::uint16_t kPort = 39091;

  std::promise<bool> server_ready;
  auto server_future = server_ready.get_future();

  std::thread server([&] {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
      server_ready.set_value(false);
      return;
    }

    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(kPort);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(server_fd, 1) < 0) {
      close(server_fd);
      server_ready.set_value(false);
      return;
    }

    server_ready.set_value(true);

    int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
      close(server_fd);
      return;
    }

    std::vector<char> buffer(kPayloadBytes);
    for (int i = 0; i < kRounds; ++i) {
      if (!RecvAll(client_fd, buffer.data(), buffer.size())) {
        break;
      }
      if (!SendAll(client_fd, buffer.data(), buffer.size())) {
        break;
      }
    }

    close(client_fd);
    close(server_fd);
  });

  if (!server_future.get()) {
    server.join();
    std::cerr << "failed to start tcp server\n";
    return 1;
  }

  int client_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (client_fd < 0) {
    server.join();
    std::cerr << "failed to create tcp client socket\n";
    return 1;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(kPort);
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

  if (connect(client_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    close(client_fd);
    server.join();
    std::cerr << "failed to connect to tcp server\n";
    return 1;
  }

  std::vector<char> payload(kPayloadBytes, 0x2A);
  std::vector<char> response(kPayloadBytes);

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < kRounds; ++i) {
    if (!SendAll(client_fd, payload.data(), payload.size())) {
      close(client_fd);
      server.join();
      std::cerr << "tcp send failed\n";
      return 1;
    }
    if (!RecvAll(client_fd, response.data(), response.size())) {
      close(client_fd);
      server.join();
      std::cerr << "tcp receive failed\n";
      return 1;
    }
  }
  const auto stop = std::chrono::steady_clock::now();

  close(client_fd);
  server.join();

  const double total_us =
      std::chrono::duration<double, std::micro>(stop - start).count();
  const double seconds = std::chrono::duration<double>(stop - start).count();
  const double throughput_mb_s =
      (2.0 * static_cast<double>(kRounds) * static_cast<double>(kPayloadBytes)) /
      (1024.0 * 1024.0 * seconds);

  std::cout << "TCP loopback comparison\n";
  std::cout << "rounds: " << kRounds << "\n";
  std::cout << "payload_bytes: " << kPayloadBytes << "\n";
  std::cout << "avg_rtt_us: " << (total_us / kRounds) << "\n";
  std::cout << "throughput_mb_s: " << throughput_mb_s << "\n";
  return 0;
}

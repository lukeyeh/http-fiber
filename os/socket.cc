#include "os/socket.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gloop/base/scheduling/domain.h"

namespace os {
namespace {

// Runs a system call that may wait, letting other fibers use the CPU
// meanwhile, and repeats it if a signal interrupted it. Leaves errno as the
// call set it.
template <typename Call>
auto Blocking(const Call call) {
  const base::scheduling::PotentiallyBlockingRegion region;
  auto result = call();
  while (result < 0 && errno == EINTR) result = call();
  return result;
}

sockaddr_in Loopback(const uint16_t port) {
  return sockaddr_in{
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)},
      .sin_zero = {},
  };
}

// The kernel takes every kind of address through this one pointer type.
const sockaddr* Generic(const sockaddr_in* const address) {
  return reinterpret_cast<const sockaddr*>(address);
}

}  // namespace

absl::StatusOr<Socket> Socket::Connect(const uint16_t port) {
  Socket socket(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (socket.fd_ < 0) return absl::ErrnoToStatus(errno, "socket");
  const sockaddr_in address = Loopback(port);
  const int connected = Blocking([&] {
    return ::connect(socket.fd_, Generic(&address), sizeof(address));
  });
  if (connected < 0) return absl::ErrnoToStatus(errno, "connect");
  return socket;
}

Socket::Socket(Socket&& other) : fd_(std::exchange(other.fd_, -1)) {}

Socket& Socket::operator=(Socket&& other) {
  std::swap(fd_, other.fd_);
  return *this;
}

Socket::~Socket() {
  if (fd_ >= 0) ::close(fd_);
}

absl::StatusOr<size_t> Socket::Read(const std::span<char> buffer) const {
  const ssize_t count =
      Blocking([&] { return ::recv(fd_, buffer.data(), buffer.size(), 0); });
  if (count >= 0) return static_cast<size_t>(count);
  if (errno == EAGAIN) return absl::DeadlineExceededError("read timed out");
  return absl::ErrnoToStatus(errno, "recv");
}

absl::Status Socket::Write(std::string_view data) const {
  while (!data.empty()) {
    // MSG_NOSIGNAL: a peer that has gone away is an error, not a signal.
    const ssize_t count = Blocking(
        [&] { return ::send(fd_, data.data(), data.size(), MSG_NOSIGNAL); });
    if (count < 0) return absl::ErrnoToStatus(errno, "send");
    data.remove_prefix(static_cast<size_t>(count));
  }
  return absl::OkStatus();
}

void Socket::FinishWriting() const { ::shutdown(fd_, SHUT_WR); }

absl::Status Socket::SetReadTimeout(
    const std::chrono::milliseconds timeout) const {
  const auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(timeout);
  const timeval value{
      .tv_sec = seconds.count(),
      .tv_usec = std::chrono::microseconds(timeout - seconds).count(),
  };
  if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value)) < 0) {
    return absl::ErrnoToStatus(errno, "setsockopt(SO_RCVTIMEO)");
  }
  return absl::OkStatus();
}

absl::StatusOr<Listener> Listener::Listen(const uint16_t port) {
  Socket socket(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (socket.fd_ < 0) return absl::ErrnoToStatus(errno, "socket");

  // Lets a restarted server take its port back without waiting for the
  // kernel to forget the previous run's connections.
  const int enable = 1;
  if (::setsockopt(socket.fd_, SOL_SOCKET, SO_REUSEADDR, &enable,
                   sizeof(enable)) < 0) {
    return absl::ErrnoToStatus(errno, "setsockopt(SO_REUSEADDR)");
  }

  sockaddr_in address = Loopback(port);
  if (::bind(socket.fd_, Generic(&address), sizeof(address)) < 0) {
    return absl::ErrnoToStatus(errno, "bind");
  }
  if (::listen(socket.fd_, SOMAXCONN) < 0) {
    return absl::ErrnoToStatus(errno, "listen");
  }

  // Ask which port was bound, in case the kernel chose it.
  socklen_t length = sizeof(address);
  if (::getsockname(socket.fd_, reinterpret_cast<sockaddr*>(&address),
                    &length) < 0) {
    return absl::ErrnoToStatus(errno, "getsockname");
  }
  return Listener(std::move(socket), ntohs(address.sin_port));
}

absl::StatusOr<Socket> Listener::Accept() const {
  const int fd = Blocking(
      [&] { return ::accept4(socket_.fd_, nullptr, nullptr, SOCK_CLOEXEC); });
  if (fd >= 0) return Socket(fd);
  // What accept reports for a listening socket that has been shut down.
  if (errno == EINVAL) return absl::CancelledError("listener shut down");
  return absl::ErrnoToStatus(errno, "accept");
}

void Listener::Shutdown() const { ::shutdown(socket_.fd_, SHUT_RDWR); }

}  // namespace os

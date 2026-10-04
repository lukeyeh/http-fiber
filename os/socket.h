// TCP sockets on the loopback interface, as objects that close themselves.
//
// This is the only place the program touches the kernel's socket interface.
// Every call that can wait (Accept, Read, Write) tells Gloop's scheduler so,
// which means a fiber may call them freely: while it waits, other fibers run.

#ifndef OS_SOCKET_H_
#define OS_SOCKET_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace os {

// One end of a TCP connection. Closes the connection when destroyed.
class Socket {
 public:
  // Connects to a listener on this machine. Fails with kUnavailable if nothing
  // is listening on `port`.
  static absl::StatusOr<Socket> Connect(uint16_t port);

  Socket(Socket&& other);
  Socket& operator=(Socket&& other);
  ~Socket();

  // Waits for data and copies up to buffer.size() bytes of it into `buffer`.
  // Returns how many bytes were copied; 0 means the peer has finished sending.
  // Fails with kDeadlineExceeded if a timeout was set and nothing arrived in
  // that time.
  absl::StatusOr<size_t> Read(std::span<char> buffer) const;

  // Sends all of `data`, waiting for the peer as long as it takes.
  absl::Status Write(std::string_view data) const;

  // Tells the peer that no more data will be sent; its Read then returns 0.
  // The socket can still be read from.
  void FinishWriting() const;

  // Limits how long each later Read waits.
  absl::Status SetReadTimeout(std::chrono::milliseconds timeout) const;

 private:
  friend class Listener;

  explicit Socket(int fd) : fd_(fd) {}

  int fd_;
};

// Accepts TCP connections made to a port on this machine. Only connections
// from this machine are possible. Stops listening when destroyed.
class Listener {
 public:
  // Starts listening on `port`, or on a free port chosen by the kernel if
  // `port` is 0. Fails with kFailedPrecondition if the port is taken.
  static absl::StatusOr<Listener> Listen(uint16_t port);

  // The port being listened on; never 0.
  uint16_t port() const { return port_; }

  // Waits for the next connection. Fails with kCancelled once Shutdown has
  // been called.
  absl::StatusOr<Socket> Accept() const;

  // Makes every Accept, including one already waiting in another thread, fail
  // with kCancelled. Safe to call from any thread.
  void Shutdown() const;

 private:
  Listener(Socket socket, uint16_t port)
      : socket_(std::move(socket)), port_(port) {}

  Socket socket_;
  uint16_t port_;
};

}  // namespace os

#endif  // OS_SOCKET_H_

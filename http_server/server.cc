#include "http_server/server.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "gloop/thread/fiber/bundle.h"
#include "gloop/thread/fiber/fiber-options.h"
#include "gloop/thread/fiber/fiber.h"
#include "gloop/thread/fiber/sleep.h"
#include "http_server/http.h"
#include "os/socket.h"

namespace http_server {
namespace {

// Longer than any honest request head; a client that sends more is refused.
constexpr size_t kMaxHeadBytes = 8192;

// How much a client may keep sending after its response before the
// connection is closed regardless.
constexpr size_t kMaxDiscardedBytes = 65536;

// How long a client may go without sending anything before it is dropped.
// This is also what bounds how long the server takes to stop.
constexpr std::chrono::seconds kReadTimeout(5);

// Reads from `socket` up to the end of a request's head and parses it. Fails
// with kInvalidArgument or kResourceExhausted if the client sent something
// that deserves an error response, and with another code if the connection
// is no longer worth answering.
absl::StatusOr<Request> ReadRequest(const os::Socket& socket) {
  ABSL_RETURN_IF_ERROR(socket.SetReadTimeout(kReadTimeout));
  std::string head;
  std::array<char, 4096> chunk{};
  while (head.find(kEndOfHead) == std::string::npos) {
    if (head.size() > kMaxHeadBytes) {
      return absl::ResourceExhaustedError("request head too large");
    }
    ABSL_ASSIGN_OR_RETURN(const size_t count, socket.Read(chunk));
    if (count == 0) return absl::UnavailableError("client closed connection");
    head.append(chunk.data(), count);
  }
  return ParseRequest(head);
}

// Ends the connection in a way that lets the client receive the response.
// Closing while the client is still sending would make the kernel discard
// what was just written, so this waits, within limits, for the client to
// finish first.
void FinishConnection(const os::Socket& socket) {
  socket.FinishWriting();
  std::array<char, 4096> discarded{};
  size_t total = 0;
  while (total <= kMaxDiscardedBytes) {
    const absl::StatusOr<size_t> count = socket.Read(discarded);
    if (!count.ok() || *count == 0) return;
    total += *count;
  }
}

}  // namespace

absl::StatusOr<std::unique_ptr<Server>> Server::Start(const uint16_t port,
                                                      Handler handler) {
  ABSL_ASSIGN_OR_RETURN(os::Listener listener, os::Listener::Listen(port));
  return absl::WrapUnique(new Server(std::move(listener), std::move(handler)));
}

Server::Server(os::Listener listener, Handler handler)
    : listener_(std::move(listener)),
      handler_(std::move(handler)),
      // A root fiber: the server's fibers form their own tree, independent of
      // whichever thread or fiber started it.
      accept_fiber_(thread::Fiber::RootFiber{}, thread::TreeOptions(),
                    [this] { AcceptConnections(); }) {}

Server::~Server() {
  accept_fiber_.Cancel();
  // Cancelling does not interrupt a fiber that is waiting in Accept.
  listener_.Shutdown();
  accept_fiber_.Join();
}

void Server::AcceptConnections() const {
  // Each connection's fiber belongs to the bundle, which frees it as soon as
  // it finishes.
  thread::Bundle connections;
  while (!thread::Cancelled()) {
    absl::StatusOr<os::Socket> socket = listener_.Accept();
    if (socket.ok()) {
      connections.Add([this, socket = std::move(*socket)]() mutable {
        ServeConnection(std::move(socket));
      });
      continue;
    }
    if (absl::IsCancelled(socket.status())) break;
    // Most likely the process is out of file descriptors. Give connections a
    // moment to finish rather than spinning.
    LOG(WARNING) << "accepting a connection failed: " << socket.status();
    thread::CancellableSleepFor(absl::Milliseconds(100));
  }
  connections.JoinAll();
}

void Server::ServeConnection(const os::Socket socket) const {
  const absl::StatusOr<Request> request = ReadRequest(socket);
  Response response;
  if (request.ok()) {
    response = handler_(*request);
  } else if (absl::IsInvalidArgument(request.status())) {
    response = Response{
        .status = 400,
        .body = "Bad Request\n",
    };
  } else if (absl::IsResourceExhausted(request.status())) {
    response = Response{
        .status = 431,
        .body = "Request Header Fields Too Large\n",
    };
  } else {
    // The client went away or stalled; there is nobody to answer.
    return;
  }
  // A client that stopped listening for its response has lost nothing.
  socket.Write(SerializeResponse(response)).IgnoreError();
  FinishConnection(socket);
}

}  // namespace http_server

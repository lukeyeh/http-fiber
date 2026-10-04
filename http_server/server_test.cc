// Shows how to start a Server and what a client sees from it, including that
// one slow client does not hold up the others.

#include "http_server/server.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "http_server/http.h"
#include "os/socket.h"

namespace http_server {
namespace {

using absl_testing::StatusIs;
using testing::EndsWith;
using testing::StartsWith;

// Answers every request with its own method and target.
Response Echo(const Request& request) {
  return Response{
      .body = request.method + " " + request.target,
  };
}

std::unique_ptr<Server> StartEchoServer() {
  // Port 0 gives each test its own free port.
  absl::StatusOr<std::unique_ptr<Server>> server = Server::Start(0, Echo);
  CHECK_OK(server);
  return std::move(*server);
}

// Sends `request` to the server on `port` and returns everything it sends
// back before closing the connection.
std::string Exchange(const uint16_t port, const std::string_view request) {
  const absl::StatusOr<os::Socket> socket = os::Socket::Connect(port);
  CHECK_OK(socket);
  CHECK_OK(socket->Write(request));

  std::string reply;
  std::array<char, 1024> chunk{};
  while (true) {
    const absl::StatusOr<size_t> count = socket->Read(chunk);
    CHECK_OK(count);
    if (*count == 0) return reply;
    reply.append(chunk.data(), *count);
  }
}

TEST(ServerTest, AnswersARequestWithTheHandlersResponse) {
  const std::unique_ptr<Server> server = StartEchoServer();

  EXPECT_EQ(Exchange(server->port(), "GET /hello HTTP/1.1\r\n\r\n"),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 10\r\n"
            "Connection: close\r\n"
            "\r\n"
            "GET /hello");
}

// Each connection carries one request, so a client makes a new connection
// for the next one.
TEST(ServerTest, ServesOneRequestAfterAnother) {
  const std::unique_ptr<Server> server = StartEchoServer();

  EXPECT_THAT(Exchange(server->port(), "GET /1 HTTP/1.1\r\n\r\n"),
              EndsWith("GET /1"));
  EXPECT_THAT(Exchange(server->port(), "POST /2 HTTP/1.1\r\n\r\n"),
              EndsWith("POST /2"));
}

// The point of a fiber per connection: this client connects and then sends
// nothing, and a second client is still served straight away.
TEST(ServerTest, AStalledClientDoesNotHoldUpOthers) {
  const std::unique_ptr<Server> server = StartEchoServer();
  const absl::StatusOr<os::Socket> stalled =
      os::Socket::Connect(server->port());
  ABSL_ASSERT_OK(stalled);

  EXPECT_THAT(Exchange(server->port(), "GET /prompt HTTP/1.1\r\n\r\n"),
              EndsWith("GET /prompt"));
}

// The handler is not called for a request that cannot be parsed.
TEST(ServerTest, AnswersAMalformedRequestWith400) {
  const std::unique_ptr<Server> server = StartEchoServer();

  EXPECT_THAT(Exchange(server->port(), "nonsense\r\n\r\n"),
              StartsWith("HTTP/1.1 400 Bad Request\r\n"));
}

TEST(ServerTest, AnswersAnOversizedHeadWith431) {
  const std::unique_ptr<Server> server = StartEchoServer();
  const std::string request =
      "GET / HTTP/1.1\r\nX-Padding: " + std::string(16384, 'x') + "\r\n\r\n";

  EXPECT_THAT(Exchange(server->port(), request), StartsWith("HTTP/1.1 431 "));
}

TEST(ServerTest, StartFailsIfThePortIsTaken) {
  const std::unique_ptr<Server> first = StartEchoServer();

  EXPECT_THAT(Server::Start(first->port(), Echo),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

// Destroying the server is how to stop it; afterwards nothing is listening.
TEST(ServerTest, StopsListeningWhenDestroyed) {
  std::unique_ptr<Server> server = StartEchoServer();
  const uint16_t port = server->port();
  server.reset();

  EXPECT_THAT(os::Socket::Connect(port),
              StatusIs(absl::StatusCode::kUnavailable));
}

}  // namespace
}  // namespace http_server

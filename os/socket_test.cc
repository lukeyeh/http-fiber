// Shows how to listen for, make and use TCP connections with os::Socket and
// os::Listener, and which errors to expect when the other side is not there.

#include "os/socket.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace os {
namespace {

using absl_testing::IsOkAndHolds;
using absl_testing::StatusIs;

// Both ends of one connection.
struct Pair {
  Socket client;
  Socket server;
};

Pair ConnectedPair() {
  const absl::StatusOr<Listener> listener = Listener::Listen(0);
  CHECK_OK(listener);
  absl::StatusOr<Socket> client = Socket::Connect(listener->port());
  CHECK_OK(client);
  absl::StatusOr<Socket> server = listener->Accept();
  CHECK_OK(server);
  return Pair{
      .client = std::move(*client),
      .server = std::move(*server),
  };
}

// Asking for port 0 gets a real port, which is how tests avoid collisions.
TEST(ListenerTest, PortZeroPicksAFreePort) {
  const absl::StatusOr<Listener> listener = Listener::Listen(0);
  ABSL_ASSERT_OK(listener);
  EXPECT_NE(listener->port(), 0);
}

TEST(ListenerTest, ListeningOnATakenPortFails) {
  const absl::StatusOr<Listener> first = Listener::Listen(0);
  ABSL_ASSERT_OK(first);
  EXPECT_THAT(Listener::Listen(first->port()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

// Shutdown is how another thread stops a loop that is waiting in Accept.
TEST(ListenerTest, ShutdownWakesAWaitingAccept) {
  const absl::StatusOr<Listener> listener = Listener::Listen(0);
  ABSL_ASSERT_OK(listener);

  absl::Status accepted;
  std::thread waiter([&] { accepted = listener->Accept().status(); });
  listener->Shutdown();
  waiter.join();

  EXPECT_THAT(accepted, StatusIs(absl::StatusCode::kCancelled));
}

TEST(SocketTest, ConnectingToAPortNobodyListensOnFails) {
  // Take a free port, then stop listening on it.
  uint16_t port = 0;
  {
    const absl::StatusOr<Listener> listener = Listener::Listen(0);
    ABSL_ASSERT_OK(listener);
    port = listener->port();
  }
  EXPECT_THAT(Socket::Connect(port), StatusIs(absl::StatusCode::kUnavailable));
}

TEST(SocketTest, DataWrittenAtOneEndIsReadAtTheOther) {
  const Pair pair = ConnectedPair();
  ABSL_ASSERT_OK(pair.client.Write("ping"));

  std::array<char, 16> buffer{};
  ASSERT_THAT(pair.server.Read(buffer), IsOkAndHolds(4));
  EXPECT_EQ(std::string(buffer.data(), 4), "ping");
}

// A read of 0 bytes is the end of the stream, not an error.
TEST(SocketTest, ReadReturnsZeroOnceThePeerFinishesWriting) {
  const Pair pair = ConnectedPair();
  pair.client.FinishWriting();

  std::array<char, 16> buffer{};
  EXPECT_THAT(pair.server.Read(buffer), IsOkAndHolds(0));
}

// Closing a socket ends the stream for the peer in the same way.
TEST(SocketTest, DestroyingASocketEndsThePeersStream) {
  Pair pair = ConnectedPair();
  {
    const Socket closed = std::move(pair.client);
  }

  std::array<char, 16> buffer{};
  EXPECT_THAT(pair.server.Read(buffer), IsOkAndHolds(0));
}

// Without a timeout this Read would wait for ever, as the client sends nothing.
TEST(SocketTest, ReadGivesUpAfterTheTimeout) {
  const Pair pair = ConnectedPair();
  ABSL_ASSERT_OK(pair.server.SetReadTimeout(std::chrono::milliseconds(20)));

  std::array<char, 16> buffer{};
  EXPECT_THAT(pair.server.Read(buffer),
              StatusIs(absl::StatusCode::kDeadlineExceeded));
}

}  // namespace
}  // namespace os

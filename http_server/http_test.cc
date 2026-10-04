// Shows which requests ParseRequest accepts and what SerializeResponse puts
// on the wire, then measures what each costs per message.

#include "http_server/http.h"

#include <benchmark/benchmark.h>

#include <string>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace http_server {
namespace {

using absl_testing::StatusIs;

TEST(ParseRequestTest, ReadsMethodAndTarget) {
  const absl::StatusOr<Request> request =
      ParseRequest("GET /a/b?x=1 HTTP/1.1\r\nHost: example\r\n\r\n");
  ABSL_ASSERT_OK(request);
  EXPECT_EQ(request->method, "GET");
  // The query string is part of the target; it is not split off.
  EXPECT_EQ(request->target, "/a/b?x=1");
}

TEST(ParseRequestTest, RejectsARequestLineWithMissingParts) {
  EXPECT_THAT(ParseRequest("GET /\r\n\r\n"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ParseRequestTest, RejectsATargetThatIsNotAPath) {
  EXPECT_THAT(ParseRequest("GET index.html HTTP/1.1\r\n\r\n"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ParseRequestTest, RejectsOtherProtocols) {
  EXPECT_THAT(ParseRequest("GET / HTTP/2\r\n\r\n"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

// The length header is what lets a client know where the body ends.
TEST(SerializeResponseTest, WritesStatusLineHeadersAndBody) {
  EXPECT_EQ(SerializeResponse(Response{
                .status = 404,
                .content_type = "text/html",
                .body = "<p>no</p>",
            }),
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 9\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<p>no</p>");
}

TEST(SerializeResponseTest, DefaultsToAnEmptyPlainTextOk) {
  EXPECT_EQ(SerializeResponse(Response{}),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n");
}

// A head about the size a browser sends; only its first line is looked at.
void BM_ParseRequest(benchmark::State& state) {
  const std::string head =
      "GET /index.html HTTP/1.1\r\n"
      "Host: localhost:8080\r\n"
      "User-Agent: curl/8.0\r\n"
      "Accept: */*\r\n"
      "\r\n";
  for (auto _ : state) {
    benchmark::DoNotOptimize(ParseRequest(head));
  }
}
BENCHMARK(BM_ParseRequest);

void BM_SerializeResponse(benchmark::State& state) {
  const Response response{
      .body = std::string(1024, 'x'),
  };
  for (auto _ : state) {
    benchmark::DoNotOptimize(SerializeResponse(response));
  }
}
BENCHMARK(BM_SerializeResponse);

}  // namespace
}  // namespace http_server

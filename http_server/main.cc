// Serves a greeting over HTTP on localhost until killed.
//
//   bazel run //http_server -- --port=8080
//   curl localhost:8080

#include <cstdint>
#include <iostream>
#include <memory>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/statusor.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "http_server/http.h"
#include "http_server/server.h"

ABSL_FLAG(uint16_t, port, 8080, "The port to serve on; 0 picks a free one.");

namespace {

http_server::Response Greet(const http_server::Request& request) {
  if (request.target != "/") {
    return http_server::Response{
        .status = 404,
        .body = "Not Found\n",
    };
  }
  return http_server::Response{
      .body = "Hello from a fiber!\n",
  };
}

}  // namespace

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  const absl::StatusOr<std::unique_ptr<http_server::Server>> server =
      http_server::Server::Start(absl::GetFlag(FLAGS_port), Greet);
  if (!server.ok()) {
    std::cerr << server.status() << "\n";
    return 1;
  }
  std::cout << "Serving on http://localhost:" << (*server)->port() << "\n"
            << std::flush;

  // The server runs in its own fibers; this thread has nothing left to do.
  absl::SleepFor(absl::InfiniteDuration());
  return 0;
}

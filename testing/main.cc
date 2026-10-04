// The main function of every test in this repository.
//
// A _test.cc file holds two things about the code it covers: tests, which say
// whether it works, and benchmarks, which say what it costs. One binary runs
// either:
//
//   bazel test //http_server:server_test
//       Runs the tests.
//
//   bazel run -c opt //http_server:server_test -- --benchmark_filter=all
//       Runs the benchmarks instead. Any flag starting with --benchmark
//       selects this mode; --benchmark_filter takes a regular expression to
//       choose which ones.

#include <benchmark/benchmark.h>

#include <string_view>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

int main(int argc, char** argv) {
  bool benchmarks_requested = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]).starts_with("--benchmark")) {
      benchmarks_requested = true;
    }
  }

  testing::InitGoogleMock(&argc, argv);
  if (!benchmarks_requested) return RUN_ALL_TESTS();

  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}

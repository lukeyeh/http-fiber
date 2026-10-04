// The smallest file that uses //testing:main: one test and one benchmark.
// Copy its shape for a new _test.cc.

#include <benchmark/benchmark.h>

#include "gtest/gtest.h"

namespace {

// `bazel test //testing:main_test` runs this and not the benchmark.
TEST(MainTest, RunsTestsByDefault) { EXPECT_EQ(1 + 1, 2); }

// `bazel run -c opt //testing:main_test -- --benchmark_filter=all` runs this
// and not the test.
void BM_Nothing(benchmark::State& state) {
  for (auto _ : state) {
    int value = 0;
    benchmark::DoNotOptimize(value);
  }
}
BENCHMARK(BM_Nothing);

}  // namespace

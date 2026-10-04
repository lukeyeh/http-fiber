// Checks that a binary linking //testing:main can hold tests and benchmarks
// side by side, and runs only the kind that was asked for.

#include <benchmark/benchmark.h>

#include "gtest/gtest.h"

namespace {

bool benchmark_ran = false;

// `bazel test //testing:main_test` runs this, and by then no benchmark has.
TEST(MainTest, RunsTestsAndNotBenchmarksByDefault) {
  EXPECT_FALSE(benchmark_ran);
}

// `bazel run //testing:main_test -- --benchmark_filter=all` runs this and
// not the test.
void BM_Nothing(benchmark::State& state) {
  benchmark_ran = true;
  for (auto _ : state) {
    int value = 0;
    benchmark::DoNotOptimize(value);
  }
}
BENCHMARK(BM_Nothing);

}  // namespace

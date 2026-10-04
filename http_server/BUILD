load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:private"])

# Run with: bazel run //http_server -- --port=8080
cc_binary(
    name = "http_server",
    srcs = ["main.cc"],
    deps = [
        ":http",
        ":server",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/time",
    ],
)

cc_library(
    name = "http",
    srcs = ["http.cc"],
    hdrs = ["http.h"],
    deps = [
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "http_test",
    srcs = ["http_test.cc"],
    deps = [
        ":http",
        "//testing:main",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "server",
    srcs = ["server.cc"],
    hdrs = ["server.h"],
    deps = [
        ":http",
        "//os:socket",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_macros",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/time",
        "@gloop//gloop/thread/fiber",
    ],
)

cc_test(
    name = "server_test",
    srcs = ["server_test.cc"],
    deps = [
        ":http",
        ":server",
        "//os:socket",
        "//testing:main",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@googletest//:gtest",
    ],
)

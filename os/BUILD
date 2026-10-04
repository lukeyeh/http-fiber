load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

# The only package that talks to the kernel. Everything above it uses these
# classes instead of system calls.
package(default_visibility = ["//http_server:__pkg__"])

cc_library(
    name = "socket",
    srcs = ["socket.cc"],
    hdrs = ["socket.h"],
    deps = [
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@gloop//gloop/base/scheduling",
    ],
)

cc_test(
    name = "socket_test",
    srcs = ["socket_test.cc"],
    deps = [
        ":socket",
        "//testing:main",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@googletest//:gtest",
    ],
)

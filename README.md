# http-fiber

A small HTTP/1.1 server in which every connection is served by its own
[Gloop fiber](https://github.com/lukeyeh/gloop/blob/nix/gloop/thread/fiber/fiber.h).
The code that handles a connection is plain blocking code, read a request,
write a response, and a connection that is waiting for its client costs the
others nothing.

```
nix develop                                 # or `direnv allow` once
bazel run //http_server -- --port=8080
curl localhost:8080                         # Hello from a fiber!
```

Bazel must be run inside the Nix shell; outside it, Bazel silently uses the
host compiler, which Gloop rejects. The first build takes a few minutes while
Bazel builds Gloop and Abseil and Nix builds Google Benchmark.

## How it works

```
Server
  └─ accepting fiber          waits in Listener::Accept
       └─ Bundle
            ├─ connection fiber    read request → handler → write response
            ├─ connection fiber
            └─ ...
```

- **One fiber accepts.** `Server::Start` creates a root `thread::Fiber` that
  loops on `os::Listener::Accept`.
- **One fiber per connection.** Each accepted socket is handed to a new fiber
  in a `thread::Bundle`, which frees the fiber as soon as it finishes.
- **Waiting does not block the others.** Gloop schedules fibers
  cooperatively, so a fiber must say when it is about to wait in the kernel.
  `os/socket.cc` wraps every such call in a `PotentiallyBlockingRegion`,
  which is the one place that has to know this.
- **Destroying the server stops it.** The destructor cancels the accepting
  fiber, shuts the listener down to wake it out of `Accept`, and joins it,
  which waits for the requests in progress.

## Using the server

```cpp
#include "http_server/server.h"

http_server::Response Greet(const http_server::Request& request) {
  return http_server::Response{
      .body = "you asked for " + request.target,
  };
}

absl::StatusOr<std::unique_ptr<http_server::Server>> server =
    http_server::Server::Start(8080, Greet);
```

The handler may be called from several fibers at once. Passing port 0 picks
a free port, which `server->port()` reports. `http_server/server_test.cc`
shows the rest, from a client's point of view.

## Limits

It is deliberately barebones:

- It listens on the loopback interface only, so only this machine can reach
  it.
- One request per connection; every response says `Connection: close`.
- Only the request line is read. Headers are not interpreted and a request
  body is ignored.
- A request head over 8 KB is answered with 431, a malformed request line
  with 400, and a client that sends nothing for 5 seconds is dropped.
- The program has no signal handling; it runs until killed.

## Layout

Dependencies point downwards, and Bazel `visibility` enforces it.

| Path | Purpose |
| --- | --- |
| `http_server/main.cc` | The `http_server` program: flags and the greeting handler |
| `http_server/server.h` | `Server`: the accepting fiber and the fiber per connection |
| `http_server/http.h` | `Request`, `Response` and the HTTP wire format |
| `os/socket.h` | `Socket` and `Listener`; the only code that makes system calls |
| `testing/main.cc` | The `main` every test links |

Every `foo.h`/`foo.cc` has a `foo_test.cc` beside it, written to be read as
documentation for `foo`.

## Everyday commands

| To | Run |
| --- | --- |
| Run all tests | `bazel test //...` |
| Run the benchmarks in a test file | `bazel run -c opt //http_server:server_test -- --benchmark_filter=all` |
| Generate `compile_commands.json` for clangd | `bazel run :compile_commands` |
| Format | `clang-format -i */*.cc */*.h` and `buildifier -r .` |
| Lint | `clang-tidy */*.cc` (after generating compile commands) |
| Flamegraph of serving requests | `bazel run --config=flamegraph //http_server:server_test -- --benchmark_filter=all`, then open `/tmp/flamegraph.svg` |

A `foo_test.cc` holds the tests for `foo` and, below them, its benchmarks.
The binary runs the tests by default and the benchmarks when given any
`--benchmark...` flag.

Start clangd with `--query-driver=/**/*` so that it can find Nix's system
headers.

## Build environment

Nix pins the compiler, the tools and the nixpkgs libraries; Bazel builds the
code and fetches Gloop. Gloop's requirements decide most of the setup; its
[NIX.md](https://github.com/lukeyeh/gloop/blob/nix/NIX.md) has the details.

- **The dev shell comes from Gloop's flake.** Gloop only compiles with clang
  and libc++, never libstdc++. `gloop.lib.mkDevShell` in `flake.nix` provides
  that toolchain with Bazel configured for it.
- **`.bazelrc` sets `-fno-exceptions` and `-funsigned-char`**, which Gloop
  insists on. Nothing in this repository throws; failures are `absl::Status`.
- **There is no `nix build`.** Deploy the binary Bazel builds,
  `bazel-bin/http_server/http_server`.

| From | Libraries | Depend on them as |
| --- | --- | --- |
| [lukeyeh/gloop](https://github.com/lukeyeh/gloop), by commit (`MODULE.bazel`) | Gloop | `@gloop//gloop/thread/fiber` |
| Bazel registry (`MODULE.bazel`) | Abseil, GoogleTest | `@abseil-cpp//absl/status`, `@googletest//:gtest` |
| nixpkgs (`nix/deps.nix`) | Google Benchmark | `@gbenchmark` |

Abseil and GoogleTest come from the registry at the versions Gloop itself
uses, so that a binary holds one copy of each.

### Updating Gloop

The fork follows upstream daily. Put a newer commit of its `nix` branch in
the `git_override` in `MODULE.bazel` and run `nix flake update gloop`, so
that the code and the dev shell move together. If Gloop's `MODULE.bazel` has
raised its Abseil or GoogleTest version, raise them here to match.

### Adding a library

- **From Gloop:** add its target to `deps`; the names are in the `BUILD` file
  next to each header in the fork.
- **From nixpkgs:** add the package to `nix/deps.nix` (a C++ library must be
  rebuilt against libc++, as `gbenchmark` is there), add a
  `nix_pkg.file(...)` block for it in `MODULE.bazel`, and depend on it as
  `"@<name>"`.

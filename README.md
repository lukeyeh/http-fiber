# http-fiber

A C++ project built with Bazel inside a Nix dev shell, laid out after
[nix-bazel-cpp](https://github.com/lukeyeh/nix-bazel-cpp-skill), that uses
[Gloop](https://github.com/lukeyeh/gloop). It is a small HTTP/1.1 server in
which every connection is served by its own
[Gloop fiber](https://github.com/lukeyeh/gloop/blob/nix/gloop/thread/fiber/fiber.h).

Nix pins the compiler, the tools and the nixpkgs libraries; Bazel builds the
code, and fetches Gloop.

## Setup

```
nix develop                  # or `direnv allow` once
bazel test //...
bazel run //http_server -- --port=8080      # then: curl localhost:8080
```

Bazel must be run inside the Nix shell; outside it, Bazel silently uses the
host compiler, which Gloop rejects. The first build takes a few minutes while
Bazel builds Abseil and Nix builds Google Benchmark.

## Everyday commands

| To | Run |
| --- | --- |
| Run all tests | `bazel test //...` |
| Run one test file's benchmarks | `bazel run -c opt //testing:main_test -- --benchmark_filter=all` |
| Generate `compile_commands.json` for clangd | `bazel run :compile_commands` |
| Format | `clang-format -i */*.cc */*.h` and `buildifier -r .` |
| Lint | `clang-tidy */*.cc` (after generating compile commands) |
| Flamegraph of a target | `bazel run --config=flamegraph //testing:main_test -- --benchmark_filter=all`, then open `/tmp/flamegraph.svg` |

Start clangd with `--query-driver=/**/*` so that it can find Nix's system
headers.

## Tests and benchmarks

Each `foo_test.cc` holds the tests for `foo` and, below them, its benchmarks.
Every test links `//testing:main`, which runs the tests by default and the
benchmarks when given any `--benchmark...` flag. `testing/main_test.cc` is the
smallest example.

## What Gloop changes

Three things differ from a nix-bazel-cpp project without Gloop. Gloop's
[NIX.md](https://github.com/lukeyeh/gloop/blob/nix/NIX.md) has the details.

1.  **The dev shell comes from Gloop's flake.** Gloop only compiles with
    clang and libc++, never libstdc++. `gloop.lib.mkDevShell` in `flake.nix`
    is `pkgs.mkShell` with that toolchain and Bazel configured for it.
2.  **Gloop, Abseil and GoogleTest come from the Bazel registry, not
    nixpkgs.** Gloop exists only as a Bazel module and builds its own Abseil;
    taking the same ones keeps a single copy in each binary. Everything else
    still comes from nixpkgs.
3.  **There is no Nix package.** `nix build` in nix-bazel-cpp compiles the
    sources without Bazel, which cannot work for code that needs Gloop.
    Deploy the binary Bazel builds.

`.bazelrc` also carries the two flags Gloop insists on, `-fno-exceptions` and
`-funsigned-char`.

## Libraries

| From | Libraries | Depend on them as |
| --- | --- | --- |
| The fork, by commit (`MODULE.bazel`) | Gloop | `@gloop//gloop/thread/fiber` |
| Bazel registry (`MODULE.bazel`) | Abseil, GoogleTest | `@abseil-cpp//absl/strings`, `@googletest//:gtest` |
| nixpkgs (`nix/deps.nix`) | Google Benchmark, and anything you add | `@gbenchmark` |

### Using another part of Gloop

Add its target to `deps`, for example `"@gloop//gloop/util/math:mathutil"`.
The target names are in the `BUILD` file next to each header in the fork.

### Adding a library from nixpkgs

1. Add the package to `nix/deps.nix`. A C library goes in as it is. A C++
   library must be rebuilt against libc++, as `gbenchmark` is there.
2. Add a `nix_pkg.file(...)` block for it in `MODULE.bazel`, copying the
   existing one, and its name to the `use_repo` line. List shared libraries by
   their versioned name, e.g. `srcs = ["lib/liburing.so.2"]`.
3. Depend on it in a `BUILD` file as `"@<name>"`.

Do not add Abseil, GoogleTest or protobuf this way; see above.

### Updating Gloop

The fork follows upstream daily. To move to a newer Gloop, put a newer commit
of its `nix` branch in the `git_override` in `MODULE.bazel` and run
`nix flake update gloop`, so that the code and the dev shell move together.
If Gloop's `MODULE.bazel` has raised its Abseil or GoogleTest version, raise
them here to match.

## Layout

| Path | Purpose |
| --- | --- |
| `http_server/server.h` | The server: an accepting fiber and a fiber per connection |
| `http_server/http.h` | Parsing requests and writing responses |
| `http_server/main.cc` | The `http_server` program |
| `os/socket.h` | TCP sockets; the only code that makes system calls |
| `testing/main.cc` | The `main` of every test |
| `nix/deps.nix` | The list of libraries taken from nixpkgs |
| `nix/bazel.nix` | Presents those libraries to Bazel |
| `MODULE.bazel` | Gloop, the registry libraries and the nixpkgs imports |
| `.bazelrc` | Build flags and profiling configs |
| `flake.nix`, `flake.lock` | The dev shell and the pin for nixpkgs and Gloop's flake |

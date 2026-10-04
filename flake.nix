{
  description = "http-fiber: a C++ project built with Bazel and Nix that uses Gloop";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";

    # Supplies the dev shell: Gloop only compiles with clang and libc++, and
    # its flake knows how to set Bazel up for that.
    gloop.url = "github:lukeyeh/gloop";
    # One nixpkgs, so one compiler pin.
    gloop.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { self, nixpkgs, gloop }:
    let
      # Gloop only supports Linux on x86-64 and ARM64.
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      devShells = forAllSystems (pkgs: {
        # Bazel 9, buildifier, clang with libc++ and the matching clang tools,
        # plus the packages listed here.
        default = gloop.lib.mkDevShell pkgs {
          packages = [
            # Profiling: perf samples a running program, and flamegraph
            # (from cargo-flamegraph) runs perf and draws the result.
            pkgs.perf
            pkgs.cargo-flamegraph
            # samply opens perf's recordings in the Firefox Profiler, an
            # interactive viewer that runs in the browser.
            pkgs.samply
            # pprof is an alternative viewer, also usable in the terminal.
            # perf_data_converter lets it read perf's recordings, and
            # graphviz draws its call graphs.
            pkgs.pprof
            pkgs.perf_data_converter
            pkgs.graphviz
          ];
        };
      });
    };
}

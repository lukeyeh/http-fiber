# The libraries Bazel takes from nixpkgs, at the versions flake.lock pins.
#
# Gloop, Abseil and GoogleTest are not here. Gloop only exists as a Bazel
# module and builds its own Abseil, so those come from the Bazel registry; see
# MODULE.bazel.
#
# Everything is compiled against libc++, because Gloop refuses libstdc++. A C
# library can be listed as it is (`liburing = pkgs.liburing;`). A C++ library
# has to be rebuilt with the libc++ stdenv, as below.
pkgs:
let
  # The same LLVM version as the dev shell (Gloop's flake.nix).
  libcxxStdenv = pkgs.llvmPackages_21.libcxxStdenv;
in
{
  # For benchmarks only.
  gbenchmark = pkgs.gbenchmark.override {
    stdenv = libcxxStdenv;
    # Its own tests link GoogleTest, which must then be libc++ too.
    gtest = pkgs.gtest.override { stdenv = libcxxStdenv; };
  };
}

#!/usr/bin/env bash
# Stage a native Ubuntu 22.04 ARM64 build for S3/npm. glibc 2.35 is the release baseline;
# unlike x86_64 this target does not use manylinux2014.
set -euo pipefail

source_dir="${1:-build/lib}"
dest_dir="${2:-build/native-aarch64/lib}"
mkdir -p "$dest_dir"
for file in libquiver.so libquiver.so.0 libquiver_c.so; do
  # Dereference CMake's versioned symlinks so every uploaded object is complete.
  cp -L --remove-destination "$source_dir/$file" "$dest_dir/$file"
done
# DT_RPATH, not DT_RUNPATH: ld.so searches LD_LIBRARY_PATH before RUNPATH, so a RUNPATH would let
# a user's LD_LIBRARY_PATH pair this libquiver_c.so with a different build's libquiver.so.0.
patchelf --force-rpath --set-rpath '$ORIGIN' "$dest_dir/libquiver_c.so"

# Julia's bundled libstdc++ provides GLIBCXX <= 3.4.30; a newer default compiler would exceed it.
bad_cxx="$(objdump -T "$dest_dir"/*.so* | sed -nE 's/.*GLIBCXX_([0-9.]+).*/\1/p' | awk -F. '$1>3 || ($1==3 && ($2>4 || ($2==4 && $3>30)))')"
[ -z "$bad_cxx" ] || { echo "ERROR: GLIBCXX symbols above 3.4.30: $bad_cxx" >&2; exit 1; }

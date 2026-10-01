#!/usr/bin/env bash
# Stage a native Ubuntu 22.04 ARM64 build for S3/npm. GCC 11 and glibc 2.35 are
# the release baseline; unlike x86_64 this target does not use manylinux2014.
set -euo pipefail

source_dir="${1:-build/lib}"
dest_dir="${2:-build/native-aarch64/lib}"
mkdir -p "$dest_dir"
libs=()
for file in libquiver.so libquiver.so.0 libquiver_c.so; do
  # Dereference CMake's versioned symlinks so every uploaded object is complete.
  cp -L --remove-destination "$source_dir/$file" "$dest_dir/$file"
  libs+=("$dest_dir/$file")
done
# DT_RPATH, not DT_RUNPATH: ld.so searches LD_LIBRARY_PATH before RUNPATH, so a RUNPATH would let
# a user's LD_LIBRARY_PATH pair this libquiver_c.so with a different build's libquiver.so.0.
patchelf --force-rpath --set-rpath '$ORIGIN' "$dest_dir/libquiver_c.so"

for lib in "${libs[@]}"; do
  readelf -h "$lib" | grep -E 'Machine:[[:space:]]+AArch64' > /dev/null || {
    echo "ERROR: $lib is not an AArch64 ELF library" >&2; exit 1;
  }
  objdump -p "$lib" | grep 'NEEDED.*libstdc++\.so\.6' > /dev/null || {
    echo "ERROR: $lib is not dynamically linked to libstdc++" >&2; exit 1;
  }
done
syms="$(objdump -T "${libs[@]}")"
bad_glibc="$(printf '%s\n' "$syms" | sed -nE 's/.*GLIBC_([0-9.]+).*/\1/p' | awk -F. '$1>2 || ($1==2 && ($2>35 || ($2==35 && $3>0)))')"
bad_cxx="$(printf '%s\n' "$syms" | sed -nE 's/.*GLIBCXX_([0-9.]+).*/\1/p' | awk -F. '$1>3 || ($1==3 && ($2>4 || ($2==4 && $3>30)))')"
[ -z "$bad_glibc" ] || { echo "ERROR: glibc symbols above 2.35: $bad_glibc" >&2; exit 1; }
[ -z "$bad_cxx" ] || { echo "ERROR: GLIBCXX symbols above 3.4.30: $bad_cxx" >&2; exit 1; }
objdump -p "$dest_dir/libquiver_c.so" | grep -E 'RPATH[[:space:]]+\$ORIGIN[[:space:]]*$' > /dev/null || {
  echo 'ERROR: libquiver_c.so missing $ORIGIN DT_RPATH' >&2; exit 1;
}
echo 'OK: AArch64, glibc<=2.35, GLIBCXX<=3.4.30, libstdc++ dynamic, $ORIGIN rpath set'

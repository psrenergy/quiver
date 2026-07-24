#!/usr/bin/env bash
# Build the portable Linux native libs (libquiver.so[.0], libquiver_c.so) with a glibc 2.17 floor,
# inside the manylinux2014 image (CentOS 7 = glibc 2.17, GCC 10 -> GLIBCXX_3.4.28, dynamic libstdc++).
#
# Runs IDENTICALLY locally and in CI -- it only needs Docker:
#   - Locally (Windows Git Bash / macOS / Linux, with Docker running):  bash scripts/build_native_linux.sh
#   - CI: .github/workflows/publish-s3.yml invokes it on the Linux runner.
#
# Output: build/manylinux/lib/{libquiver.so, libquiver.so.0, libquiver_c.so} -- real files (not
# symlinks), with an $ORIGIN rpath on libquiver_c.so so it finds libquiver.so.0 as a sibling. Builds
# into build/manylinux/ (a dedicated, gitignored dir) so it never clobbers a native build in build/.
set -euo pipefail

# Pinned by digest so the toolchain / cmake / glibc floor can't drift. manylinux2014_x86_64 is always
# the CentOS 7 (glibc 2.17) image and ships GCC 10 (-> GLIBCXX_3.4.28); CentOS 7's SCL caps at GCC 11,
# so GLIBCXX can never exceed the 3.4.30 ceiling Julia's bundled libstdc++ provides.
IMAGE="quay.io/pypa/manylinux2014_x86_64@sha256:0d25b049964b2549b83384036abdff06789a8c0b1e9ff003ec80f0d531f79e50"

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Docker Desktop on Windows/Git-Bash needs a native path (C:/...); `pwd -W` yields that there and is an
# error elsewhere (fall back to the POSIX path on Linux/macOS).
host="$(cd "$repo" && { pwd -W 2>/dev/null || pwd; })"

# MSYS_NO_PATHCONV stops Git Bash from rewriting the container-side /paths into Windows paths.
# --mount (not -v) avoids the Windows drive-letter colon being mis-parsed as the mount separator.
MSYS_NO_PATHCONV=1 docker run --rm -i --platform linux/amd64 \
  --mount "type=bind,source=${host},target=/io" -w /io \
  "$IMAGE" bash -s <<'INNER'
set -exo pipefail
if [ -f /opt/rh/devtoolset-10/enable ]; then source /opt/rh/devtoolset-10/enable; fi
gcc --version
cmake --version

cmake -B build/manylinux \
    -DCMAKE_BUILD_TYPE=Release \
    -DQUIVER_BUILD_TESTS=OFF \
    -DQUIVER_BUILD_C_API=ON
cmake --build build/manylinux --parallel "$(nproc)"

# libquiver_c.so must find libquiver.so.0 as a sibling in the flat ship layout (what BinaryBuilder's
# ELF auditor used to do), then turn the version symlinks into real files (matching the old cp -L).
patchelf --set-rpath '$ORIGIN' "$(readlink -f build/manylinux/lib/libquiver_c.so)"
( cd build/manylinux/lib
  for f in libquiver.so libquiver.so.0 libquiver_c.so; do
    if [ -L "$f" ]; then cp --remove-destination "$(readlink -f "$f")" "$f"; fi
  done )

# Portability gate: glibc <= 2.17, GLIBCXX <= 3.4.30, libstdc++ dynamic, $ORIGIN rpath present.
libs=(build/manylinux/lib/libquiver.so build/manylinux/lib/libquiver.so.0 build/manylinux/lib/libquiver_c.so)
syms="$(objdump -T "${libs[@]}")"
bad_glibc="$(printf '%s\n' "$syms" | grep -oE 'GLIBC_[0-9]+\.[0-9]+(\.[0-9]+)?' | sed 's/GLIBC_//' | sort -uV | awk -F. '$1>2 || ($1==2 && $2>17)')"
[ -z "$bad_glibc" ] || { echo "ERROR: glibc symbols above 2.17: $bad_glibc"; exit 1; }
bad_cxx="$(printf '%s\n' "$syms" | grep -oE 'GLIBCXX_[0-9]+\.[0-9]+(\.[0-9]+)?' | sed 's/GLIBCXX_//' | sort -uV | awk -F. '$1>3 || ($1==3 && ($2>4 || ($2==4 && $3>30)))')"
[ -z "$bad_cxx" ] || { echo "ERROR: GLIBCXX symbols above 3.4.30: $bad_cxx"; exit 1; }
for l in "${libs[@]}"; do
  objdump -p "$l" | grep -q 'NEEDED.*libstdc++\.so\.6' || { echo "ERROR: $l is not dynamically linked to libstdc++"; exit 1; }
done
objdump -p build/manylinux/lib/libquiver_c.so | grep -Eq 'R(UN)?PATH.*\$ORIGIN' || { echo "ERROR: libquiver_c.so missing \$ORIGIN rpath"; exit 1; }
echo "OK: glibc<=2.17, GLIBCXX<=3.4.30, libstdc++ dynamic, \$ORIGIN rpath set"
INNER

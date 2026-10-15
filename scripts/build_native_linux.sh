#!/usr/bin/env bash
# Build the portable Linux native libs (libquiver.so[.0], libquiver_c.so) inside a manylinux image:
#   x86_64  (default): manylinux2014  (CentOS 7 = glibc 2.17, GCC 11 -> GLIBCXX_3.4.29)
#   aarch64:           manylinux_2_28 (AlmaLinux 8 = glibc 2.28) -- CentOS 7's aarch64 SCL stops at
#                      GCC 10, which lacks the C++20 <chrono> calendar the core needs.
# Both keep libstdc++ dynamic.
#
# Runs IDENTICALLY locally and in CI -- it only needs Docker:
#   - Locally (Windows Git Bash / macOS / Linux, with Docker running):  bash scripts/build_native_linux.sh [aarch64]
#   - CI: .github/workflows/publish-s3.yml invokes it on the Linux runners.
#
# Output: build/manylinux-<arch>/lib/{libquiver.so, libquiver.so.0, libquiver_c.so} -- real files (not
# symlinks), with an $ORIGIN rpath on libquiver_c.so so it finds libquiver.so.0 as a sibling. Builds
# into a dedicated, gitignored dir per arch so it never clobbers a native build in build/.
set -euo pipefail

ARCH="${1:-x86_64}"

# Pinned by digest so the toolchain / cmake / glibc floor can't drift. On x86_64 we build with GCC 11
# (-> GLIBCXX_3.4.29), the newest toolset CentOS 7's SCL offers, so GLIBCXX stays <= the 3.4.30
# ceiling Julia's bundled libstdc++ provides. manylinux_2_28's gcc-toolset links newer libstdc++
# symbols statically (libstdc++_nonshared), so GLIBCXX stays at the system's 3.4.25.
case "$ARCH" in
  x86_64)
    IMAGE="quay.io/pypa/manylinux2014_x86_64@sha256:0d25b049964b2549b83384036abdff06789a8c0b1e9ff003ec80f0d531f79e50"
    DOCKER_PLATFORM="linux/amd64"
    GLIBC_MINOR=17 ;;
  aarch64)
    IMAGE="quay.io/pypa/manylinux_2_28_aarch64@sha256:162c81dfd3efc710732a571717d3c916a6945ebf279e879ddee3243af96fe46f"
    DOCKER_PLATFORM="linux/arm64"
    GLIBC_MINOR=28 ;;
  *) echo "usage: build_native_linux.sh [x86_64|aarch64]" >&2; exit 2 ;;
esac

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Docker Desktop on Windows/Git-Bash needs a native path (C:/...); `pwd -W` yields that there and is an
# error elsewhere (fall back to the POSIX path on Linux/macOS).
host="$(cd "$repo" && { pwd -W 2>/dev/null || pwd; })"

# MSYS_NO_PATHCONV stops Git Bash from rewriting the container-side /paths into Windows paths.
# --mount (not -v) avoids the Windows drive-letter colon being mis-parsed as the mount separator.
MSYS_NO_PATHCONV=1 docker run --rm -i --platform "$DOCKER_PLATFORM" \
  -e ARCH="$ARCH" -e GLIBC_MINOR="$GLIBC_MINOR" -e OUT="build/manylinux-$ARCH" \
  --mount "type=bind,source=${host},target=/io" -w /io \
  "$IMAGE" bash -s <<'INNER'
set -exo pipefail
# GCC 11 (not 10): the core uses the C++20 <chrono> calendar (year_month_day / hh_mm_ss / sys_days),
# which GCC 10 lacks. Install-if-missing since the manylinux2014 image's default toolset is GCC 10;
# devtoolset-11 is the newest CentOS 7 SCL offers (-> GLIBCXX_3.4.29, still <= the 3.4.30 ceiling).
# manylinux_2_28's default gcc-toolset is already new enough.
if [ "$ARCH" = x86_64 ]; then
  yum install -y devtoolset-11-gcc-c++ devtoolset-11-libatomic-devel
  source /opt/rh/devtoolset-11/enable
fi
gcc --version
cmake --version

cmake -B "$OUT" \
    -DCMAKE_BUILD_TYPE=Release \
    -DQUIVER_BUILD_TESTS=OFF \
    -DQUIVER_BUILD_C_API=ON
cmake --build "$OUT" --parallel "$(nproc)"

# libquiver_c.so must find libquiver.so.0 as a sibling in the flat ship layout (what BinaryBuilder's
# ELF auditor used to do), then turn the version symlinks into real files (matching the old cp -L).
patchelf --set-rpath '$ORIGIN' "$(readlink -f "$OUT/lib/libquiver_c.so")"
( cd "$OUT/lib"
  for f in libquiver.so libquiver.so.0 libquiver_c.so; do
    if [ -L "$f" ]; then cp --remove-destination "$(readlink -f "$f")" "$f"; fi
  done )

# Portability gate: glibc <= 2.$GLIBC_MINOR, GLIBCXX <= 3.4.30, libstdc++ dynamic, $ORIGIN rpath present.
libs=("$OUT/lib/libquiver.so" "$OUT/lib/libquiver.so.0" "$OUT/lib/libquiver_c.so")
syms="$(objdump -T "${libs[@]}")"
bad_glibc="$(printf '%s\n' "$syms" | grep -oE 'GLIBC_[0-9]+\.[0-9]+(\.[0-9]+)?' | sed 's/GLIBC_//' | sort -uV | awk -F. -v m="$GLIBC_MINOR" '$1>2 || ($1==2 && $2>m)')"
[ -z "$bad_glibc" ] || { echo "ERROR: glibc symbols above 2.$GLIBC_MINOR: $bad_glibc"; exit 1; }
bad_cxx="$(printf '%s\n' "$syms" | grep -oE 'GLIBCXX_[0-9]+\.[0-9]+(\.[0-9]+)?' | sed 's/GLIBCXX_//' | sort -uV | awk -F. '$1>3 || ($1==3 && ($2>4 || ($2==4 && $3>30)))')"
[ -z "$bad_cxx" ] || { echo "ERROR: GLIBCXX symbols above 3.4.30: $bad_cxx"; exit 1; }
# Consume all output: grep -q can give objdump SIGPIPE and fail the check under pipefail.
for l in "${libs[@]}"; do
  objdump -p "$l" | grep 'NEEDED.*libstdc++\.so\.6' >/dev/null || { echo "ERROR: $l is not dynamically linked to libstdc++"; exit 1; }
done
objdump -p "$OUT/lib/libquiver_c.so" | grep -E 'R(UN)?PATH.*\$ORIGIN' >/dev/null || { echo "ERROR: libquiver_c.so missing \$ORIGIN rpath"; exit 1; }
echo "OK: glibc<=2.$GLIBC_MINOR, GLIBCXX<=3.4.30, libstdc++ dynamic, \$ORIGIN rpath set"
INNER

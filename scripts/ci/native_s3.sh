#!/usr/bin/env bash
#
# Single source of truth for moving Quiver's *raw* native libraries through S3, keyed by
# version. The release pipeline builds the libs once (publish.yml), uploads them here,
# and every consumer (publish-js.yml, publish-julia.yml) downloads them here. Because S3
# persists across workflow runs, each publish workflow is independently runnable: a standalone
# `publish-js` just downloads the libs for its version, no rebuild and no in-run artifact.
#
# This is DISTINCT from the Julia shipping tarballs that scripts/julia/generate_artifacts.jl
# uploads (quiver-<tag>.tgz, 2 stems only). Those omit the libquiver.so.0 SONAME that the npm
# loader resolves at load time, so npm cannot reuse them — hence this raw-lib key space.
#
# Layout (mirrors generate_artifacts.jl's bucket/prefix/version scheme):
#   s3://<bucket>/<prefix>/<version>/native/<platform>/<file>
# Objects are uploaded --acl public-read, so downloads are anonymous HTTPS (no AWS creds).
# Uploads need S3 write — run them on the self-hosted runner with the ambient IAM role.
#
# Env overrides (same defaults as generate_artifacts.jl):
#   QUIVER_S3_BUCKET (default "julia-artifacts"), QUIVER_S3_PREFIX (default "quiver").
#
# Usage:
#   scripts/ci/native_s3.sh upload   <version>          # reads native/<platform>/<file>
#   scripts/ci/native_s3.sh download <version> <dest>   # writes <dest>/<platform>/<file>

set -euo pipefail

S3_BUCKET="${QUIVER_S3_BUCKET:-julia-artifacts}"
S3_PREFIX="${QUIVER_S3_PREFIX:-quiver}"

readonly PLATFORMS=("linux-x86_64" "windows-x86_64")

# Canonical per-platform native-lib file set. linux ships the unversioned name AND the SONAME
# libquiver.so.0 (libquiver_c.so's DT_NEEDED resolves it via RPATH=$ORIGIN); windows ships the
# two runtime DLLs. This list is the contract between producer (publish.yml) and consumers
# (js/julia) — keep it in lockstep with publish.yml's matrix lib_paths.
files_for() {
  case "$1" in
    linux-x86_64)   echo "libquiver.so libquiver.so.0 libquiver_c.so" ;;
    windows-x86_64) echo "libquiver.dll libquiver_c.dll" ;;
    *) echo "::error::native_s3.sh: unknown platform '$1'" >&2; return 1 ;;
  esac
}

s3_object() {  # <version> <platform> <file>  ->  <prefix>/<version>/native/<platform>/<file>
  echo "${S3_PREFIX}/$1/native/$2/$3"
}

cmd_upload() {
  local version="$1" platform file src
  for platform in "${PLATFORMS[@]}"; do
    for file in $(files_for "$platform"); do
      src="native/${platform}/${file}"
      if [ ! -f "$src" ]; then
        echo "::error::native_s3.sh upload: missing local file '$src'" >&2
        exit 1
      fi
      aws s3 cp "$src" "s3://${S3_BUCKET}/$(s3_object "$version" "$platform" "$file")" --acl public-read
    done
  done
  echo "Uploaded native libs for v${version} to s3://${S3_BUCKET}/${S3_PREFIX}/${version}/native/"
}

cmd_download() {
  local version="$1" dest="$2" platform file url
  for platform in "${PLATFORMS[@]}"; do
    mkdir -p "${dest}/${platform}"
    for file in $(files_for "$platform"); do
      url="https://${S3_BUCKET}.s3.amazonaws.com/$(s3_object "$version" "$platform" "$file")"
      if ! curl -fSL "$url" -o "${dest}/${platform}/${file}"; then
        echo "::error::native lib not found in S3: ${url}" >&2
        echo "::error::no native libs for v${version} — run the publish workflow for this version first" >&2
        exit 1
      fi
    done
  done
  echo "Downloaded native libs for v${version} into ${dest}/"
}

case "${1:-}" in
  upload)   shift; cmd_upload "$@" ;;
  download) shift; cmd_download "$@" ;;
  *)
    echo "usage: native_s3.sh {upload <version> | download <version> <dest>}" >&2
    exit 2
    ;;
esac

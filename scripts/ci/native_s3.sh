#!/usr/bin/env bash

set -euo pipefail

S3_BUCKET="${QUIVER_S3_BUCKET:-julia-artifacts}"
S3_PREFIX="${QUIVER_S3_PREFIX:-quiver}"

readonly PLATFORMS=("linux-x86_64" "macos-x86_64" "windows-x86_64")

files_for() {
  case "$1" in
    linux-x86_64)   echo "libquiver.so libquiver.so.0 libquiver_c.so" ;;
    macos-x86_64)   echo "libquiver.dylib libquiver_c.dylib" ;;
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

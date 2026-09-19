#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")/.."
rm -rf .dart_tool/hooks_runner .dart_tool/lib
dart test "$@"

#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")/.."
dart test "$@"

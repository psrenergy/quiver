#!/bin/bash

BASEPATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
export LD_LIBRARY_PATH="$BASEPATH/../../build/lib:$LD_LIBRARY_PATH"

cd "$BASEPATH"
dart pub global activate coverage
dart pub global run coverage:test_with_coverage --out=coverage

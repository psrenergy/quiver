#!/bin/bash

BASEPATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

cd "$BASEPATH"
dart pub global activate coverage
dart pub global run coverage:test_with_coverage --out=coverage

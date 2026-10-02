@echo off

SET BASE_PATH=%~dp0..

pushd %BASE_PATH%
dart pub global activate coverage
dart pub global run coverage:test_with_coverage --out=coverage
popd

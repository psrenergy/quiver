@echo off

SET BASE_PATH=%~dp0..

pushd %BASE_PATH%
dart run ffigen
popd

@echo off
pushd %~dp0..
rmdir /s /q .dart_tool\hooks_runner 2>nul
rmdir /s /q .dart_tool\lib 2>nul
dart test %*
popd

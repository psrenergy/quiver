@echo off
pushd %~dp0..
dart test %*
popd

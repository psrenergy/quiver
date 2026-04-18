@echo off
pushd %~dp0..
set PATH=%~dp0..\..\..\build\bin;%PATH%
deno test --allow-ffi --allow-read --allow-write --allow-env test/ %*
popd

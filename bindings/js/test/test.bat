@echo off
pushd %~dp0..
set PATH=%~dp0..\..\..\build\bin;%PATH%
bun test test/ %*
popd

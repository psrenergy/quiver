@echo off
pushd %~dp0..
set PATH=%~dp0..\..\..\build\bin;%PATH%
uv run pytest tests/ %*
popd

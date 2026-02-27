@echo off
pushd %~dp0..
uv run python generator/generator.py %*
popd

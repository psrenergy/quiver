@echo off
cd /d %~dp0
call ..\.venv\Scripts\python.exe generate_bindings.py

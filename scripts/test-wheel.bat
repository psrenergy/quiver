@echo off
setlocal enabledelayedexpansion

:: Build a Python wheel from bindings/python/ and validate its contents.
:: Run from repo root: scripts\test-wheel.bat
:: Requires: uv (Python package manager)

set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%.."
set "PYTHON_DIR=%REPO_ROOT%\bindings\python"
set "DIST_DIR=%PYTHON_DIR%\dist"
set "VENV_PYTHON=%PYTHON_DIR%\.venv\Scripts\python.exe"
set "EXIT_CODE=0"

echo ============================================================
echo  Wheel Build and Validation
echo ============================================================
echo.

:: Clean previous dist
if exist "%DIST_DIR%" (
    echo Cleaning previous dist directory...
    rmdir /s /q "%DIST_DIR%"
)

:: Build the wheel using uv
echo Building wheel from bindings/python/...
echo.
pushd "%PYTHON_DIR%"
uv build --wheel
if !errorlevel! neq 0 (
    echo.
    echo FAIL: uv build --wheel failed with exit code !errorlevel!
    popd
    exit /b 1
)
popd

:: Find the wheel file
set "WHEEL_FILE="
for %%f in ("%DIST_DIR%\quiverdb-*.whl") do (
    set "WHEEL_FILE=%%f"
)

if not defined WHEEL_FILE (
    echo FAIL: No quiverdb wheel found in %DIST_DIR%
    exit /b 1
)

echo.
echo Found wheel: !WHEEL_FILE!
echo.

:: Validate wheel contents
echo Validating wheel contents...
echo.
"%VENV_PYTHON%" "%SCRIPT_DIR%validate_wheel.py" "!WHEEL_FILE!"

if !errorlevel! neq 0 (
    set "EXIT_CODE=1"
)

:: Clean up
echo.
echo Cleaning up dist directory...
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"

echo.
if !EXIT_CODE! equ 0 (
    echo ============================================================
    echo  SUCCESS: Wheel build and validation passed
    echo ============================================================
) else (
    echo ============================================================
    echo  FAILURE: Wheel validation failed
    echo ============================================================
)

exit /b !EXIT_CODE!

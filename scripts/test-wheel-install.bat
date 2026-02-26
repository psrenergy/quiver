@echo off
setlocal enabledelayedexpansion

:: Build a Python wheel, install it in a clean venv, and verify bundled library loading.
:: Run from repo root: scripts\test-wheel-install.bat
:: Requires: uv (Python package manager)
::
:: This validates that `import quiverdb` loads native libraries from the bundled
:: _libs/ directory WITHOUT any PATH setup -- proving the wheel is self-contained.

set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%.."
set "PYTHON_DIR=%REPO_ROOT%\bindings\python"
set "DIST_DIR=%PYTHON_DIR%\dist"
set "TEMP_VENV=%TEMP%\quiver_wheel_test_%RANDOM%"
set "VENV_PYTHON=%TEMP_VENV%\Scripts\python.exe"
set "EXIT_CODE=0"

echo ============================================================
echo  Wheel Install Validation (End-to-End)
echo ============================================================
echo.

:: ---- Step 1: Build the wheel ----
if exist "%DIST_DIR%" (
    echo Cleaning previous dist directory...
    rmdir /s /q "%DIST_DIR%"
)

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

:: ---- Step 2: Create a clean temporary venv ----
echo Creating clean virtual environment at %TEMP_VENV%...
uv venv "%TEMP_VENV%"
if !errorlevel! neq 0 (
    echo FAIL: uv venv failed
    exit /b 1
)
echo.

:: ---- Step 3: Install the wheel (no PATH setup for build/bin/) ----
echo Installing wheel into clean venv...
uv pip install --python "%VENV_PYTHON%" "!WHEEL_FILE!"
if !errorlevel! neq 0 (
    echo FAIL: wheel install failed
    goto :cleanup
)
echo.

:: ---- Step 4: Run import validation ----
echo Running import validation...
echo.
"%VENV_PYTHON%" "%SCRIPT_DIR%validate_wheel_install.py"
if !errorlevel! neq 0 (
    echo.
    echo FAIL: Import validation failed
    set "EXIT_CODE=1"
    goto :cleanup
)
echo.

:: ---- Step 5: Install pytest and run the full test suite ----
echo Installing pytest into venv...
uv pip install --python "%VENV_PYTHON%" pytest
if !errorlevel! neq 0 (
    echo FAIL: pytest install failed
    set "EXIT_CODE=1"
    goto :cleanup
)
echo.

echo Running full test suite against wheel-installed package...
echo (NOTE: build/bin/ is NOT in PATH -- bundled libs must work on their own)
echo.
"%VENV_PYTHON%" -m pytest "%PYTHON_DIR%\tests" -v
if !errorlevel! neq 0 (
    echo.
    echo FAIL: Test suite failed
    set "EXIT_CODE=1"
    goto :cleanup
)

:cleanup
:: ---- Step 6: Clean up ----
echo.
echo Cleaning up...
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
if exist "%TEMP_VENV%" rmdir /s /q "%TEMP_VENV%"

echo.
if !EXIT_CODE! equ 0 (
    echo ============================================================
    echo  SUCCESS: Wheel install validation passed
    echo  - import quiverdb works without PATH setup
    echo  - _load_source = bundled
    echo  - Full test suite passed
    echo ============================================================
) else (
    echo ============================================================
    echo  FAILURE: Wheel install validation failed
    echo ============================================================
)

exit /b !EXIT_CODE!

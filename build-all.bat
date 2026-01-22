@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM Build and Test All - Quiver
REM ============================================================
REM Builds C++ library, C API, and runs all tests:
REM   - C++ unit tests
REM   - C API tests
REM   - Julia binding tests
REM   - Dart binding tests
REM ============================================================

SET ROOT_DIR=%~dp0
SET BUILD_TYPE=Debug

REM Parse arguments
:parse_args
if "%~1"=="" goto end_parse
if /i "%~1"=="--release" (
    SET BUILD_TYPE=Release
    shift
    goto parse_args
)
if /i "%~1"=="--debug" (
    SET BUILD_TYPE=Debug
    shift
    goto parse_args
)
if /i "%~1"=="--help" (
    goto show_help
)
shift
goto parse_args
:end_parse

echo.
echo ============================================================
echo  Quiver - Build All (%BUILD_TYPE%)
echo ============================================================
echo.

REM ============================================================
REM Step 1: Build C++ Library and C API
REM ============================================================
echo [1/5] Building C++ library and C API...
echo.

cmake -B build -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON
if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed
    exit /b 1
)

cmake --build build --config %BUILD_TYPE%
if errorlevel 1 (
    echo.
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo [1/5] Build completed successfully
echo.

REM ============================================================
REM Step 2: Run C++ Tests
REM ============================================================
echo [2/5] Running C++ tests...
echo.

"%ROOT_DIR%build\bin\quiver_tests.exe"
if errorlevel 1 (
    echo.
    echo ERROR: C++ tests failed
    exit /b 1
)

echo.
echo [2/5] C++ tests passed
echo.

REM ============================================================
REM Step 3: Run C API Tests
REM ============================================================
echo [3/5] Running C API tests...
echo.

"%ROOT_DIR%build\bin\quiver_c_tests.exe"
if errorlevel 1 (
    echo.
    echo ERROR: C API tests failed
    exit /b 1
)

echo.
echo [3/5] C API tests passed
echo.

REM ============================================================
REM Step 4: Run Julia Tests
REM ============================================================
echo [4/5] Running Julia tests...
echo.

pushd "%ROOT_DIR%bindings\julia\test"
call test.bat
set JULIA_EXIT=%errorlevel%
popd

if %JULIA_EXIT% neq 0 (
    echo.
    echo ERROR: Julia tests failed
    exit /b 1
)

echo.
echo [4/5] Julia tests passed
echo.

REM ============================================================
REM Step 5: Run Dart Tests
REM ============================================================
echo [5/5] Running Dart tests...
echo.

pushd "%ROOT_DIR%bindings\dart\test"
call test.bat
set DART_EXIT=%errorlevel%
popd

if %DART_EXIT% neq 0 (
    echo.
    echo ERROR: Dart tests failed
    exit /b 1
)

echo.
echo [5/5] Dart tests passed
echo.

REM ============================================================
REM Summary
REM ============================================================
echo ============================================================
echo  All builds and tests completed successfully!
echo ============================================================
echo.
echo   C++ library:  OK
echo   C API:        OK
echo   C++ tests:    OK
echo   C API tests:  OK
echo   Julia tests:  OK
echo   Dart tests:   OK
echo.

exit /b 0

:show_help
echo Usage: build-all.bat [options]
echo.
echo Options:
echo   --debug     Build in Debug mode (default)
echo   --release   Build in Release mode
echo   --help      Show this help message
echo.
exit /b 0

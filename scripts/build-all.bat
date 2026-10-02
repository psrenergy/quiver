@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM Build and Test All - Quiver
REM ============================================================
REM Configures and builds the C++ library and C API, then runs
REM scripts\test-all.bat (all six test suites, with a summary).
REM ============================================================

SET ROOT_DIR=%~dp0..

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
REM Build C++ Library and C API
REM ============================================================
echo [build] Building C++ library and C API...
echo.

cmake -S "%ROOT_DIR%" -B "%ROOT_DIR%\build" -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON
if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed
    exit /b 1
)

cmake --build "%ROOT_DIR%\build" --config %BUILD_TYPE%
if errorlevel 1 (
    echo.
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo [build] Build completed successfully
echo.

REM ============================================================
REM Tests: the one runner (all six suites, with a summary)
REM ============================================================
call "%ROOT_DIR%\scripts\test-all.bat"
exit /b %errorlevel%

:show_help
echo Usage: scripts\build-all.bat [options]
echo.
echo Options:
echo   --debug     Build in Debug mode (default)
echo   --release   Build in Release mode
echo   --help      Show this help message
echo.
exit /b 0

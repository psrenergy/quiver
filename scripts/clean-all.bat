@echo off
setlocal

REM ============================================================
REM Clean All - Quiver
REM ============================================================
REM Removes build artifacts and caches across all layers:
REM   - C++ build output
REM   - Dart binding caches
REM   - Julia resolved dependencies
REM   - Python build/dist/cache directories
REM ============================================================

SET ROOT_DIR=%~dp0..

echo.
echo ============================================================
echo  Quiver - Clean All
echo ============================================================
echo.

REM ============================================================
REM Step 1: C++ Build
REM ============================================================
echo [1/4] C++ build output...

if exist "%ROOT_DIR%\build" (
    rmdir /s /q "%ROOT_DIR%\build"
    echo       Removed build/
) else (
    echo       Skipped build/ (not found)
)

echo.

REM ============================================================
REM Step 2: Dart Bindings
REM ============================================================
echo [2/4] Dart binding caches...

if exist "%ROOT_DIR%\bindings\dart\.dart_tool" (
    rmdir /s /q "%ROOT_DIR%\bindings\dart\.dart_tool"
    echo       Removed bindings/dart/.dart_tool/
) else (
    echo       Skipped bindings/dart/.dart_tool/ (not found)
)

if exist "%ROOT_DIR%\bindings\dart\pubspec.lock" (
    del /q "%ROOT_DIR%\bindings\dart\pubspec.lock"
    echo       Removed bindings/dart/pubspec.lock
) else (
    echo       Skipped bindings/dart/pubspec.lock (not found)
)

echo.

REM ============================================================
REM Step 3: Julia Bindings
REM ============================================================
echo [3/4] Julia resolved dependencies...

if exist "%ROOT_DIR%\bindings\julia\Manifest.toml" (
    del /q "%ROOT_DIR%\bindings\julia\Manifest.toml"
    echo       Removed bindings/julia/Manifest.toml
) else (
    echo       Skipped bindings/julia/Manifest.toml (not found)
)

echo.

REM ============================================================
REM Step 4: Python Bindings
REM ============================================================
echo [4/4] Python build and cache directories...

if exist "%ROOT_DIR%\bindings\python\build" (
    rmdir /s /q "%ROOT_DIR%\bindings\python\build"
    echo       Removed bindings/python/build/
) else (
    echo       Skipped bindings/python/build/ (not found)
)

if exist "%ROOT_DIR%\bindings\python\dist" (
    rmdir /s /q "%ROOT_DIR%\bindings\python\dist"
    echo       Removed bindings/python/dist/
) else (
    echo       Skipped bindings/python/dist/ (not found)
)

if exist "%ROOT_DIR%\bindings\python\.pytest_cache" (
    rmdir /s /q "%ROOT_DIR%\bindings\python\.pytest_cache"
    echo       Removed bindings/python/.pytest_cache/
) else (
    echo       Skipped bindings/python/.pytest_cache/ (not found)
)

REM Remove __pycache__ directories recursively under bindings/python/
for /d /r "%ROOT_DIR%\bindings\python" %%d in (__pycache__) do (
    if exist "%%d" (
        rmdir /s /q "%%d"
        echo       Removed %%d
    )
)

REM Remove *.egg-info directories under bindings/python/src/
for /d %%d in ("%ROOT_DIR%\bindings\python\src\*.egg-info") do (
    if exist "%%d" (
        rmdir /s /q "%%d"
        echo       Removed %%d
    )
)

echo.

REM ============================================================
REM Summary
REM ============================================================
echo ============================================================
echo  Clean completed.
echo ============================================================
echo.

exit /b 0

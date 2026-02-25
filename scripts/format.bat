@echo off
setlocal

SET ROOT=%~dp0..

echo Formatting C/C++ code...
cmake --build "%ROOT%\build" --target format
if %ERRORLEVEL% neq 0 (
    echo Failed to format C/C++ code
    exit /b 1
)

echo.
echo Formatting Julia bindings...
pushd "%ROOT%\bindings\julia\format"
call format.bat
popd
if %ERRORLEVEL% neq 0 (
    echo Failed to format Julia bindings
    exit /b 1
)

echo.
echo Formatting Dart bindings...
pushd "%ROOT%\bindings\dart"
call format.bat
popd
if %ERRORLEVEL% neq 0 (
    echo Failed to format Dart bindings
    exit /b 1
)

echo.
echo Formatting Python bindings...
pushd "%ROOT%\bindings\python"
call format.bat
popd
if %ERRORLEVEL% neq 0 (
    echo Failed to format Python bindings
    exit /b 1
)

echo.
echo All formatting complete.

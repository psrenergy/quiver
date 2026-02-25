@echo off
setlocal

SET ROOT=%~dp0..

echo Generating Julia FFI bindings...
pushd "%ROOT%\bindings\julia\generator"
call generator.bat
popd
if %ERRORLEVEL% neq 0 (
    echo Failed to generate Julia FFI bindings
    exit /b 1
)

echo.
echo Generating Dart FFI bindings...
pushd "%ROOT%\bindings\dart\generator"
call generator.bat
popd
if %ERRORLEVEL% neq 0 (
    echo Failed to generate Dart FFI bindings
    exit /b 1
)

echo.
echo Generating Python FFI bindings...
pushd "%ROOT%\bindings\python\generator"
call generator.bat
popd
if %ERRORLEVEL% neq 0 (
    echo Failed to generate Python FFI bindings
    exit /b 1
)

echo.
echo All FFI bindings generated.

@echo off
setlocal enabledelayedexpansion

SET ROOT=%~dp0..
SET BUILD=%ROOT%\build
SET FAILED=0

echo Running clang-tidy...

REM Strip MinGW-specific flags that clang-tidy doesn't understand
REM (cmake configure regenerates compile_commands.json, so in-place edit is safe)
powershell -Command "(Get-Content '%BUILD%\compile_commands.json') -replace '-fno-keep-inline-dllexport','' | Set-Content '%BUILD%\compile_commands.json'"

echo.

for /r "%ROOT%\src" %%f in (*.cpp) do (
    echo "%%f" | findstr /i "\\blob\\" >nul
    if !ERRORLEVEL! neq 0 (
        echo   %%~nxf
        clang-tidy -p "%BUILD%" --header-filter="(include/quiver/|src/)" "%%f" 2>nul
        if !ERRORLEVEL! neq 0 set FAILED=1
    )
)

if %FAILED% neq 0 (
    echo.
    echo clang-tidy found issues
    exit /b 1
)

echo.
echo Static analysis complete.

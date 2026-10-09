@echo off

SET ROOT=%~dp0..
SET BUILD=%ROOT%\build
SET RUN_CLANG_TIDY=
for /f "delims=" %%i in ('where run-clang-tidy 2^>nul') do if not defined RUN_CLANG_TIDY SET "RUN_CLANG_TIDY=%%i"
if not defined RUN_CLANG_TIDY (
    echo Error: run-clang-tidy not found on PATH
    exit /b 1
)

echo Running clang-tidy...
echo.

REM Strip MinGW-specific flags that clang-tidy doesn't understand
REM (cmake configure regenerates compile_commands.json, so in-place edit is safe)
powershell -Command "(Get-Content '%BUILD%\compile_commands.json') -replace '-fno-keep-inline-dllexport','' | Set-Content '%BUILD%\compile_commands.json'"

uv run python "%RUN_CLANG_TIDY%" -p "%BUILD%" -quiet "^(?!.*[\\/]_deps[\\/]).*[\\/]src[\\/](?!binary)"
exit /b %ERRORLEVEL%

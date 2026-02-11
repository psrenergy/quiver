@echo off

SET ROOT=%~dp0..
SET BUILD=%ROOT%\build
SET RUN_CLANG_TIDY=C:\Program Files\LLVM\bin\run-clang-tidy

if not exist "%RUN_CLANG_TIDY%" (
    echo Error: run-clang-tidy not found at %RUN_CLANG_TIDY%
    exit /b 1
)

echo Running clang-tidy...
echo.

REM Strip MinGW-specific flags that clang-tidy doesn't understand
REM (cmake configure regenerates compile_commands.json, so in-place edit is safe)
powershell -Command "(Get-Content '%BUILD%\compile_commands.json') -replace '-fno-keep-inline-dllexport','' | Set-Content '%BUILD%\compile_commands.json'"

uv run python "%RUN_CLANG_TIDY%" -p "%BUILD%" -header-filter="(include/quiver/|quiver/src/)" -quiet "quiver[\\/]src[\\/](?!blob)"
exit /b %ERRORLEVEL%

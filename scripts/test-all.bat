@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM Test All - Quiver
REM ============================================================
REM Runs all tests (assumes already built):
REM   - C++ unit tests
REM   - C API tests
REM   - Julia binding tests
REM   - Dart binding tests
REM   - JavaScript binding tests
REM   - Python binding tests
REM ============================================================

SET ROOT_DIR=%~dp0..

echo.
echo ============================================================
echo  Quiver - Test All
echo ============================================================
echo.

SET FAILED=0
SET CPP_RESULT=SKIP
SET CAPI_RESULT=SKIP
SET JULIA_RESULT=SKIP
SET DART_RESULT=SKIP
SET JS_RESULT=SKIP
SET PYTHON_RESULT=SKIP
SET CLI_RESULT=SKIP

REM ============================================================
REM Step 1: Run C++ Tests
REM ============================================================
echo [1/7] Running C++ tests...

if exist "%ROOT_DIR%\build\bin\quiver_tests.exe" (
    "%ROOT_DIR%\build\bin\quiver_tests.exe"
    if errorlevel 1 (
        SET CPP_RESULT=FAIL
        SET FAILED=1
    ) else (
        SET CPP_RESULT=PASS
    )
) else (
    echo WARNING: C++ tests not found. Run scripts\build-all.bat first.
    SET CPP_RESULT=SKIP
)
echo.

REM ============================================================
REM Step 2: Run C API Tests
REM ============================================================
echo [2/7] Running C API tests...

if exist "%ROOT_DIR%\build\bin\quiver_c_tests.exe" (
    "%ROOT_DIR%\build\bin\quiver_c_tests.exe"
    if errorlevel 1 (
        SET CAPI_RESULT=FAIL
        SET FAILED=1
    ) else (
        SET CAPI_RESULT=PASS
    )
) else (
    echo WARNING: C API tests not found. Run scripts\build-all.bat first.
    SET CAPI_RESULT=SKIP
)
echo.

REM ============================================================
REM Step 3: Run Julia Tests
REM ============================================================
echo [3/7] Running Julia tests...

call "%ROOT_DIR%\bindings\julia\test\test.bat"
if errorlevel 1 (
    SET JULIA_RESULT=FAIL
    SET FAILED=1
) else (
    SET JULIA_RESULT=PASS
)
echo.

REM ============================================================
REM Step 4: Run Dart Tests
REM ============================================================
echo [4/7] Running Dart tests...

call "%ROOT_DIR%\bindings\dart\test\test.bat"
if errorlevel 1 (
    SET DART_RESULT=FAIL
    SET FAILED=1
) else (
    SET DART_RESULT=PASS
)
echo.

REM ============================================================
REM Step 5: Run JavaScript Tests
REM ============================================================
echo [5/7] Running JavaScript tests...

call "%ROOT_DIR%\bindings\js\test\test.bat"
if errorlevel 1 (
    SET JS_RESULT=FAIL
    SET FAILED=1
) else (
    SET JS_RESULT=PASS
)
echo.

REM ============================================================
REM Step 6: Run Python Tests
REM ============================================================
echo [6/7] Running Python tests...

call "%ROOT_DIR%\bindings\python\tests\test.bat"
if errorlevel 1 (
    SET PYTHON_RESULT=FAIL
    SET FAILED=1
) else (
    SET PYTHON_RESULT=PASS
)
echo.

REM ============================================================
REM Step 7: CLI smoke test
REM ============================================================
echo [7/7] Running CLI smoke test...

if exist "%ROOT_DIR%\build\bin\quiver_cli.exe" (
    SET CLI_RESULT=PASS
    "%ROOT_DIR%\build\bin\quiver_cli.exe" --schema "%ROOT_DIR%\tests\schemas\valid\collections.sql" :memory: "%ROOT_DIR%\example\example1.lua"
    if errorlevel 1 (
        echo ERROR: CLI run with valid schema and script failed
        SET CLI_RESULT=FAIL
        SET FAILED=1
    )
    REM --schema and --database are mutually exclusive: must exit with code 2
    "%ROOT_DIR%\build\bin\quiver_cli.exe" --schema a.sql --database m :memory: "%ROOT_DIR%\example\example1.lua" >nul 2>&1
    if !errorlevel! neq 2 (
        echo ERROR: CLI accepted --schema and --database together
        SET CLI_RESULT=FAIL
        SET FAILED=1
    )
) else (
    echo WARNING: quiver_cli not found. Run scripts\build-all.bat first.
    SET CLI_RESULT=SKIP
)
echo.

REM ============================================================
REM Summary
REM ============================================================
echo ============================================================
echo  Test Results
echo ============================================================
echo.
echo   C++ tests:        %CPP_RESULT%
echo   C API tests:      %CAPI_RESULT%
echo   Julia tests:      %JULIA_RESULT%
echo   Dart tests:       %DART_RESULT%
echo   JavaScript tests: %JS_RESULT%
echo   Python tests:     %PYTHON_RESULT%
echo   CLI smoke test:   %CLI_RESULT%
echo.

if %FAILED%==1 (
    echo Some tests FAILED!
    exit /b 1
) else (
    echo All tests PASSED!
    exit /b 0
)

@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM Test All - Margaux
REM ============================================================
REM Runs all tests (assumes already built):
REM   - C++ unit tests
REM   - C API tests
REM   - Julia binding tests
REM   - Dart binding tests
REM ============================================================

SET ROOT_DIR=%~dp0

echo.
echo ============================================================
echo  Margaux - Test All
echo ============================================================
echo.

SET FAILED=0
SET CPP_RESULT=SKIP
SET CAPI_RESULT=SKIP
SET JULIA_RESULT=SKIP
SET DART_RESULT=SKIP

REM ============================================================
REM Step 1: Run C++ Tests
REM ============================================================
echo [1/4] Running C++ tests...

if exist "%ROOT_DIR%build\bin\psr_database_tests.exe" (
    "%ROOT_DIR%build\bin\psr_database_tests.exe"
    if errorlevel 1 (
        SET CPP_RESULT=FAIL
        SET FAILED=1
    ) else (
        SET CPP_RESULT=PASS
    )
) else (
    echo WARNING: C++ tests not found. Run build-all.bat first.
    SET CPP_RESULT=SKIP
)
echo.

REM ============================================================
REM Step 2: Run C API Tests
REM ============================================================
echo [2/4] Running C API tests...

if exist "%ROOT_DIR%build\bin\psr_database_c_tests.exe" (
    "%ROOT_DIR%build\bin\psr_database_c_tests.exe"
    if errorlevel 1 (
        SET CAPI_RESULT=FAIL
        SET FAILED=1
    ) else (
        SET CAPI_RESULT=PASS
    )
) else (
    echo WARNING: C API tests not found. Run build-all.bat first.
    SET CAPI_RESULT=SKIP
)
echo.

REM ============================================================
REM Step 3: Run Julia Tests
REM ============================================================
echo [3/4] Running Julia tests...

pushd "%ROOT_DIR%bindings\julia\test"
call test.bat
if errorlevel 1 (
    SET JULIA_RESULT=FAIL
    SET FAILED=1
) else (
    SET JULIA_RESULT=PASS
)
popd
echo.

REM ============================================================
REM Step 4: Run Dart Tests
REM ============================================================
echo [4/4] Running Dart tests...

pushd "%ROOT_DIR%bindings\dart\test"
call test.bat
if errorlevel 1 (
    SET DART_RESULT=FAIL
    SET FAILED=1
) else (
    SET DART_RESULT=PASS
)
popd
echo.

REM ============================================================
REM Summary
REM ============================================================
echo ============================================================
echo  Test Results
echo ============================================================
echo.
echo   C++ tests:    %CPP_RESULT%
echo   C API tests:  %CAPI_RESULT%
echo   Julia tests:  %JULIA_RESULT%
echo   Dart tests:   %DART_RESULT%
echo.

if %FAILED%==1 (
    echo Some tests FAILED!
    exit /b 1
) else (
    echo All tests PASSED!
    exit /b 0
)

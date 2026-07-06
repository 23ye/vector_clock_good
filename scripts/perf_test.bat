@echo off
REM LogAgg - 百万级日志倒排索引性能压测脚本
setlocal enabledelayedexpansion

echo ===================================================
echo   LogAgg Performance Benchmark - 1,000,000 Logs
echo ===================================================
echo.

set ROOT=%~dp0..
set BUILD=%ROOT%\build
set TEST_DATA_DIR=%ROOT%\testlogs
set TARGET_FILE=%TEST_DATA_DIR%\testlogs.jsonl

echo [Step 0] Generating 1,000,000 logs...

set GEN_DIR=%TEST_DATA_DIR%

if not exist "%GEN_DIR%" mkdir "%GEN_DIR%"

%BUILD%\generate_jsonl.exe "%GEN_DIR%"

echo Log generation completed.
echo ---------------------------------------------------

if not exist "%TARGET_FILE%" (
    echo [Error] "testlogs.jsonl" not found in the %TEST_DATA_DIR% folder!
    echo        Please ensure that the million test data entries are placed in this path.
    pause
    exit /b
)

echo [Step 1] The testing environment is ready, and we are preparing to load the dataset ..
echo Loading %TARGET_FILE% and building memory inverted index, please wait ..
echo ---------------------------------------------------

echo [TestCase] Retrieve sparse/low-frequency keywords (expected latency: ^< 5ms)
echo Keyword: "calling" 
echo.
%BUILD%\query.exe -d "%TEST_DATA_DIR%" -k "calling" --sort causal -c 20
echo.
echo ---------------------------------------------------

echo.
echo ===================================================
echo   Stress testing is complete! Please check the [core query latency] of eachTestCase above.
echo ===================================================
pause
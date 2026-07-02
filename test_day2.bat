@echo off
REM Day 2 Test Script

echo ========================================
echo   Day 2 Verification Test
echo ========================================
echo.

echo [Test 1] Unit Tests
echo ----------------------------------------
build\test_vector_clock.exe
echo.

echo [Test 2] Agent Help
echo ----------------------------------------
build\agent.exe -h
echo.

echo [Test 3] UDP Receiver + Agent Test
echo ----------------------------------------
echo.
echo Instructions:
echo.
echo 1. Open a NEW terminal window and run:
echo    build\test_udp_receiver.exe
echo.
echo 2. In ANOTHER terminal window, run:
echo    build\agent.exe -n node-01 -f test\sample.log -s 127.0.0.1:9999
echo.
echo 3. Add new lines to test\sample.log and watch them appear in receiver
echo.
echo Example - Add lines to sample.log:
echo    echo "2024-06-29 10:01:00 INFO New log entry" >> test\sample.log
echo.

pause
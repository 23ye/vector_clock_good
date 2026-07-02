@echo off
REM LogAgg Build Script for Windows

set CC=gcc
set CFLAGS=-Wall -Wextra -Werror -std=c99 -g -O0 -I./include
set LDFLAGS=-lws2_32

echo Building LogAgg...

REM Create build directory
if not exist build mkdir build

REM ==================== Compile Library ====================
echo.
echo [Library]

echo [CC] src/vector_clock.c
%CC% %CFLAGS% -c src/vector_clock.c -o build/vector_clock.o
if errorlevel 1 goto :error

echo [CC] src/log_entry.c
%CC% %CFLAGS% -c src/log_entry.c -o build/log_entry.o
if errorlevel 1 goto :error

echo [CC] src/agent.c
%CC% %CFLAGS% -c src/agent.c -o build/agent.o
if errorlevel 1 goto :error

echo [CC] src/server.c
%CC% %CFLAGS% -c src/server.c -o build/server.o
if errorlevel 1 goto :error

REM Create static library
echo [AR] liblogagg.a
ar rcs build/liblogagg.a build/vector_clock.o build/log_entry.o build/agent.o build/server.o
if errorlevel 1 goto :error

REM ==================== Compile Executables ====================
echo.
echo [Executables]

echo [CC] test/test_vector_clock.c
%CC% %CFLAGS% test/test_vector_clock.c -Lbuild -llogagg -o build/test_vector_clock.exe
if errorlevel 1 goto :error

echo [CC] src/main_agent.c
%CC% %CFLAGS% src/main_agent.c -Lbuild -llogagg %LDFLAGS% -o build/agent.exe
if errorlevel 1 goto :error

echo [CC] src/main_server.c
%CC% %CFLAGS% src/main_server.c -Lbuild -llogagg %LDFLAGS% -o build/server.exe
if errorlevel 1 goto :error

echo.
echo ==============================
echo Build complete!
echo ==============================
echo.
echo Available executables:
echo   build\test_vector_clock.exe  - Run unit tests
echo   build\agent.exe              - Log collection agent
echo   build\server.exe             - Aggregation server
echo.
echo Usage:
echo   build\test_vector_clock.exe
echo   build\server.exe -p 9999 -d ./logs
echo   build\agent.exe -n node-01 -f app.log -s 127.0.0.1:9999
echo.
goto :end

:error
echo.
echo Build failed!
exit /b 1

:end
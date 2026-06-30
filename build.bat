@echo off
REM LogAgg Build Script for Windows

set CC=gcc
set CFLAGS=-Wall -Wextra -Werror -std=c99 -g -O0 -I./include

echo Building LogAgg...

REM Create build directory
if not exist build mkdir build

REM Compile source files
echo [CC] src/vector_clock.c
%CC% %CFLAGS% -c src/vector_clock.c -o build/vector_clock.o
if errorlevel 1 goto :error

echo [CC] src/log_entry.c
%CC% %CFLAGS% -c src/log_entry.c -o build/log_entry.o
if errorlevel 1 goto :error

REM Create static library
echo [AR] liblogagg.a
ar rcs build/liblogagg.a build/vector_clock.o build/log_entry.o
if errorlevel 1 goto :error

REM Compile test
echo [CC] test/test_vector_clock.c
%CC% %CFLAGS% test/test_vector_clock.c -Lbuild -llogagg -o build/test_vector_clock.exe
if errorlevel 1 goto :error

echo.
echo Build complete!
echo.
echo Run tests: build\test_vector_clock.exe
goto :end

:error
echo.
echo Build failed!
exit /b 1

:end
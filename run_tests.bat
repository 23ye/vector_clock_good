@echo off
setlocal EnableExtensions
set "ROOT=%~dp0"
cd /d "%ROOT%"

if not exist "%ROOT%build" mkdir "%ROOT%build"

if exist "%ROOT%build\test_vector_clock.exe" (
    echo [INFO] 检测到已存在测试可执行文件，直接运行。
    echo.
    "%ROOT%build\test_vector_clock.exe"
    exit /b %ERRORLEVEL%
)

where gcc >nul 2>nul
if not errorlevel 1 (
    echo [INFO] 检测到 gcc，开始构建测试程序...
    gcc -Wall -Wextra -Werror -std=c99 -g -O0 -I./include -c src/vector_clock.c -o build/vector_clock.o
    if errorlevel 1 exit /b 1

    gcc -Wall -Wextra -Werror -std=c99 -g -O0 -I./include -c src/log_entry.c -o build/log_entry.o
    if errorlevel 1 exit /b 1

    ar rcs build/liblogagg.a build/vector_clock.o build/log_entry.o
    if errorlevel 1 exit /b 1

    gcc -Wall -Wextra -Werror -std=c99 -g -O0 -I./include test/test_vector_clock.c -Lbuild -llogagg -o build/test_vector_clock.exe
    if errorlevel 1 exit /b 1

    echo.
    echo [INFO] 构建完成，正在运行测试...
    "%ROOT%build\test_vector_clock.exe"
    exit /b %ERRORLEVEL%
)

where clang >nul 2>nul
if not errorlevel 1 (
    echo [INFO] 检测到 clang，开始构建测试程序...
    clang -Wall -Wextra -Werror -std=c99 -g -O0 -I./include -c src/vector_clock.c -o build/vector_clock.o
    if errorlevel 1 exit /b 1

    clang -Wall -Wextra -Werror -std=c99 -g -O0 -I./include -c src/log_entry.c -o build/log_entry.o
    if errorlevel 1 exit /b 1

    ar rcs build/liblogagg.a build/vector_clock.o build/log_entry.o
    if errorlevel 1 exit /b 1

    clang -Wall -Wextra -Werror -std=c99 -g -O0 -I./include test/test_vector_clock.c -Lbuild -llogagg -o build/test_vector_clock.exe
    if errorlevel 1 exit /b 1

    echo.
    echo [INFO] 构建完成，正在运行测试...
    "%ROOT%build\test_vector_clock.exe"
    exit /b %ERRORLEVEL%
)

echo [ERROR] 未检测到可用的 gcc 或 clang。请先安装 MinGW-w64 或 Clang。 
exit /b 1

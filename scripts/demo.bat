@echo off
REM LogAgg Demo Script
REM Demonstrates multi-node causal log aggregation

setlocal enabledelayedexpansion

echo ========================================
echo   LogAgg Demo - Multi-Node Causal Logs
echo ========================================
echo.

set ROOT=%~dp0..
set BUILD=%ROOT%\build
set SCRIPTS=%ROOT%\scripts
set DEMO_DIR=%ROOT%\demo_data
set LOGS_DIR=%DEMO_DIR%\logs
set TEST_DIR=%ROOT%\testlogs

REM Clean previous demo data
if exist "%DEMO_DIR%" rmdir /s /q "%DEMO_DIR%"
mkdir "%DEMO_DIR%"
mkdir "%LOGS_DIR%"
mkdir "%DEMO_DIR%\agent_logs"

echo [Step 1] Creating empty log files...
echo.

REM Create empty log files for agents to monitor
type nul > "%DEMO_DIR%\agent_logs\app-node1.log"
type nul > "%DEMO_DIR%\agent_logs\app-node2.log"
type nul > "%DEMO_DIR%\agent_logs\app-node3.log"

echo [Step 2] Starting Server...
start "LogAgg Server" cmd /c "%BUILD%\server.exe -p 9999 -d %LOGS_DIR%"
timeout /t 1 /nobreak > nul

echo [Step 3] Starting Agents...
start "Agent node-01" cmd /c "%BUILD%\agent.exe -n node-01 -f %DEMO_DIR%\agent_logs\app-node1.log -s 127.0.0.1:9999 -i 200"
start "Agent node-02" cmd /c "%BUILD%\agent.exe -n node-02 -f %DEMO_DIR%\agent_logs\app-node2.log -s 127.0.0.1:9999 -i 200"
start "Agent node-03" cmd /c "%BUILD%\agent.exe -n node-03 -f %DEMO_DIR%\agent_logs\app-node3.log -s 127.0.0.1:9999 -i 200"

timeout /t 2 /nobreak > nul

echo.
echo [Step 4] Generating logs with causal relationships...
echo.
echo   Node-01: [A]------[C]--------[F]
echo   Node-02: ---[B]------[D]--
echo   Node-03: ------[E]----------[G]
echo.

REM Generate logs AFTER agents are started
echo Generating node-01 logs...
%BUILD%\gen_log.exe -d %DEMO_DIR%\agent_logs -n 1 -c 2 -i 100
echo.

echo Generating node-02 logs...
%BUILD%\gen_log.exe -d %DEMO_DIR%\agent_logs -n 2 -c 2 -i 100
echo.

echo Generating node-03 logs...
%BUILD%\gen_log.exe -d %DEMO_DIR%\agent_logs -n 3 -c 2 -i 100
echo.

echo [Step 5] Waiting for agents to send logs...
timeout /t 5 /nobreak > nul

echo.
echo [Step 6] Querying logs with causal sorting...
echo.
%BUILD%\query.exe -d %LOGS_DIR% --sort causal -c 20

echo.
echo [Step 7] Querying logs with time sorting...
echo.
%BUILD%\query.exe -d %LOGS_DIR% --sort time -c 20

echo.
echo [Step 8] Statistics...
echo.
%BUILD%\query.exe -d %LOGS_DIR% --stats

echo.
echo [Step 9] Querying logs with Inverted Index (AND Mode)...
echo   Keyword: "request completed successfully" (Must contain BOTH terms)
echo.
%BUILD%\query.exe -d %LOGS_DIR% -k "request completed successfully" --sort causal -c 20

echo.
echo [Step 10] Querying logs with Inverted Index (OR Mode)...
echo   Keyword: "info" (Contains EITHER term)
echo.
%BUILD%\query.exe -d %LOGS_DIR% -k "info" --or --sort causal -c 20

echo.
echo [Step 11] Exporting and Rendering Causal Dependency Graph...
echo.

%BUILD%\query.exe -d %LOGS_DIR% --sort causal --dot "%DEMO_DIR%\causal_network.dot"

where dot >nul 2>1
if errorlevel 1 (
    echo [Prompt] It has been detected that Graphviz is not installed locally or added to the environment variables.
    echo        Successfully generated text description script: %DEMO_DIR%\causal_network.dot
    echo        You can copy its content to https://dreampuf.github.io/GraphvizOnline for online preview!
) else (
    echo [Prompt] Rendering causal dependency graph with Graphviz...
    dot -Tpng "%DEMO_DIR%\causal_network.dot" -o "%DEMO_DIR%\causal.png"
    echo [Success] Causal dependency graph has been rendered successfully!
    echo        Image saved to: %DEMO_DIR%\causal.png
)

echo.
echo ========================================
echo   Demo Complete!
echo ========================================
echo.
echo Log files are in: %DEMO_DIR%
echo.
echo Press any key to stop all processes...
pause > nul

REM Kill background processes
taskkill /f /im server.exe > nul 2>&1
taskkill /f /im agent.exe > nul 2>&1

echo Done.
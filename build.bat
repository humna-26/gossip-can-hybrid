@echo off
set BASE=C:\Users\humna\Desktop\Parallel and Distributed Computing\PROJECT\gossip-can-hybrid
set SRC=%BASE%\src
set OUT=%BASE%\build\gossip_can_hybrid.exe

if not exist "%BASE%\build" mkdir "%BASE%\build"

g++ -std=c++17 -Wall -Wextra ^
    -I"%SRC%" ^
    "%SRC%\main.cpp" ^
    "%SRC%\simulator\EventQueue.cpp" ^
    "%SRC%\simulator\Simulator.cpp" ^
    "%SRC%\network\Node.cpp" ^
    "%SRC%\metrics\MetricsCollector.cpp" ^
    "%SRC%\can\Zone.cpp" ^
    "%SRC%\can\CANOverlay.cpp" ^
    -o "%OUT%"

if %ERRORLEVEL% EQU 0 (
    echo BUILD OK
) else (
    echo BUILD FAILED
)

@echo off
REM Build Smart Exam Lab on Windows with MinGW-w64 GCC (native).
REM Prerequisite: install MinGW-w64 (e.g. via MSYS2: pacman -S mingw-w64-x86_64-gcc make)
REM then run this script from a MinGW64 shell / terminal where gcc is on PATH.
setlocal
cd /d %~dp0..
if not exist bin mkdir bin
if not exist data mkdir data
if not exist reports mkdir reports
set CFLAGS=-Wall -Wextra -O2 -std=c11 -Iinclude -Ithird_party/sqlite -DSQLITE_THREADSAFE=1
set CORE=src\common.c src\config.c src\protocol.c src\database.c src\monitor.c src\allocation.c src\alerts.c src\reports.c third_party\sqlite\sqlite3.c
echo Building exam_server.exe ...
gcc %CFLAGS% -o bin\exam_server.exe %CORE% src\server.c src\client.c src\main_server.c -lws2_32 -lm
if errorlevel 1 goto fail
echo Building exam_client.exe ...
gcc %CFLAGS% -o bin\exam_client.exe %CORE% src\server.c src\client.c src\main_client.c -lws2_32 -lm
if errorlevel 1 goto fail
echo.
echo Build OK: bin\exam_server.exe and bin\exam_client.exe
exit /b 0
:fail
echo BUILD FAILED - read the compiler output above.
exit /b 1

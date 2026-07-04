@echo off
rem HLauncher Windows Compilation Script
echo =============================================
echo Building HLauncher for Windows
echo =============================================

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] cmake command not found. Please install CMake and add it to your PATH.
    pause
    exit /b 1
)

where git >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] git command not found. Please install Git and add it to your PATH.
    pause
    exit /b 1
)

if not exist build\windows mkdir build\windows

echo [1/2] Configuring CMake build system...
cmake -B build\windows -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b %errorlevel%
)

echo [2/2] Compiling binary...
cmake --build build\windows --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build compilation failed.
    pause
    exit /b %errorlevel%
)

echo =============================================
echo Build succeeded! Executable located at:
echo build\windows\bin\Release\HLauncher.exe
echo =============================================
pause

#!/bin/bash
# HLauncher Linux Compilation Script
set -e

echo "============================================="
echo "Building HLauncher for Linux"
echo "============================================="

# Install build dependencies warning
if ! command -v cmake &> /dev/null; then
    echo "[ERROR] cmake is required but not installed."
    echo "On Debian/Ubuntu: sudo apt install cmake g++ libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev"
    exit 1
fi

# Check for Git
if ! command -v git &> /dev/null; then
    echo "[ERROR] git is required to fetch dependencies."
    exit 1
fi

mkdir -p build/linux
echo "[1/2] Configuring CMake build system..."
cmake -B build/linux -DCMAKE_BUILD_TYPE=Release

echo "[2/2] Compiling binary..."
cmake --build build/linux --config Release

echo "============================================="
echo "Build succeeded! Executable located at:"
echo "build/linux/bin/HLauncher"
echo "============================================="

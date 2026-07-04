#!/bin/bash
# HLauncher macOS Compilation Script
set -e

echo "============================================="
echo "Building HLauncher for macOS"
echo "============================================="

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "[ERROR] cmake is required but not installed."
    echo "Please install CMake via Homebrew: brew install cmake"
    exit 1
fi

# Check for Git
if ! command -v git &> /dev/null; then
    echo "[ERROR] git is required to fetch dependencies."
    exit 1
fi

mkdir -p build/macos
echo "[1/2] Configuring CMake build system..."
cmake -B build/macos -DCMAKE_BUILD_TYPE=Release

echo "[2/2] Compiling binary..."
cmake --build build/macos --config Release

echo "============================================="
echo "Build succeeded! Application package located at:"
echo "build/macos/bin/HLauncher.app"
echo "============================================="

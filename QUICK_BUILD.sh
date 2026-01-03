#!/bin/bash
# Quick Build Script for Docker Homelab Manager
# Version: 0.4.0

set -e

echo "========================================="
echo "Docker Homelab Manager - Build Script"
echo "Version: 0.4.0"
echo "========================================="
echo ""

# Check for required tools
echo "[1/5] Checking build tools..."
command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake not found. Install it first."; exit 1; }
command -v g++ >/dev/null 2>&1 || { echo "ERROR: g++ not found. Install it first."; exit 1; }
echo "✓ CMake and compiler found"

# Check for Qt6
echo ""
echo "[2/5] Checking Qt6..."
if ! pkg-config --exists Qt6Core 2>/dev/null; then
    echo "✗ Qt6 not found!"
    echo ""
    echo "Please install Qt6 first:"
    echo "  Ubuntu/Debian: sudo apt install qt6-base-dev libsqlite3-dev libcurl4-openssl-dev"
    echo "  Fedora: sudo dnf install qt6-qtbase-devel sqlite-devel libcurl-devel"
    echo "  Arch: sudo pacman -S qt6-base sqlite curl"
    echo ""
    exit 1
fi
echo "✓ Qt6 found"

# Check for SQLite3
echo ""
echo "[3/5] Checking SQLite3..."
if ! pkg-config --exists sqlite3 2>/dev/null; then
    echo "✗ SQLite3 not found!"
    echo "Please install: sudo apt install libsqlite3-dev"
    exit 1
fi
echo "✓ SQLite3 found"

# Check for libcurl
echo ""
echo "[4/5] Checking libcurl..."
if ! pkg-config --exists libcurl 2>/dev/null; then
    echo "✗ libcurl not found!"
    echo "Please install: sudo apt install libcurl4-openssl-dev"
    exit 1
fi
echo "✓ libcurl found"

# Configure and build
echo ""
echo "[5/5] Building..."
echo ""

# Clean previous build
rm -rf build

# Configure
echo "Configuring CMake..."
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

echo ""
echo "Compiling (this may take a few minutes)..."
cmake --build build --config Release -j$(nproc)

echo ""
echo "========================================="
echo "✅ BUILD SUCCESSFUL!"
echo "========================================="
echo ""
echo "Executable: ./build/DockerHomelabManager"
echo ""
echo "To run:"
echo "  ./build/DockerHomelabManager"
echo ""
echo "To install:"
echo "  sudo cmake --install build"
echo ""

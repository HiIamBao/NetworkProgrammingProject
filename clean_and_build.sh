#!/bin/bash

# Clean and Build Script for MultiPlayer Game
# This script removes all generated build files and rebuilds the project from scratch

echo "🧹 Cleaning old build files..."

# Remove all CMake generated files
rm -rf build/
rm -rf CMakeFiles/
rm -f CMakeCache.txt
rm -f cmake_install.cmake
rm -f Makefile

echo "✅ Clean complete!"
echo ""
echo "🔨 Creating build directory..."

# Create fresh build directory
mkdir -p build
cd build

echo "⚙️  Running CMake..."

# Generate build files
cmake ..

if [ $? -ne 0 ]; then
    echo "❌ CMake failed! Please check the error messages above."
    exit 1
fi

echo "✅ CMake configuration complete!"
echo ""
echo "🔨 Building project..."

# Build the project
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "❌ Build failed! Please check the error messages above."
    exit 1
fi

echo ""
echo "✅ Build complete! Executables are in the build directory."
echo ""
echo "To run the game:"
echo "  cd build"
echo "  ./multyPlayer"

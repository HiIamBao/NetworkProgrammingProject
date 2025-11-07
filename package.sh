#!/bin/bash

# Automated Packaging Script for MultiPlayer Game
# This script creates a clean, portable distribution package

VERSION="1.0.0"
PROJECT_NAME="multiPlayerGame"
ARCHIVE_NAME="${PROJECT_NAME}-v${VERSION}-portable"

echo "📦 Packaging ${PROJECT_NAME} v${VERSION}..."
echo ""

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PARENT_DIR="$(dirname "$SCRIPT_DIR")"

# Clean local build files
echo "🧹 Cleaning local build artifacts..."
cd "$SCRIPT_DIR"
rm -rf build/ CMakeFiles/ CMakeCache.txt cmake_install.cmake Makefile *.o *.lock

if [ $? -eq 0 ]; then
    echo "✅ Local cleanup complete"
else
    echo "⚠️  Warning: Some files could not be cleaned"
fi

echo ""
echo "📦 Creating portable archive..."

# Create the archive from parent directory
cd "$PARENT_DIR"

tar --exclude='build' \
    --exclude='CMakeFiles' \
    --exclude='CMakeCache.txt' \
    --exclude='cmake_install.cmake' \
    --exclude='Makefile' \
    --exclude='*.o' \
    --exclude='*.lock' \
    --exclude='.git' \
    --exclude='.gitignore' \
    -czf "${ARCHIVE_NAME}.tar.gz" "${PROJECT_NAME}/"

if [ $? -eq 0 ]; then
    echo "✅ Archive created successfully!"
    echo ""
    echo "📊 Package Information:"
    echo "   File: ${ARCHIVE_NAME}.tar.gz"
    echo "   Location: ${PARENT_DIR}/${ARCHIVE_NAME}.tar.gz"
    echo "   Size: $(du -h "${ARCHIVE_NAME}.tar.gz" | cut -f1)"
    echo ""
    echo "📋 What's included:"
    echo "   ✅ Source code (src/, include/)"
    echo "   ✅ Build configuration (CMakeLists.txt)"
    echo "   ✅ Resources (textures, fonts, etc.)"
    echo "   ✅ Third-party libraries"
    echo "   ✅ Documentation and scripts"
    echo ""
    echo "❌ What's excluded:"
    echo "   ❌ Build artifacts (build/, Makefile, etc.)"
    echo "   ❌ Object files (*.o)"
    echo "   ❌ Lock files (*.lock)"
    echo ""
    echo "📬 Sharing Instructions:"
    echo "   1. Share ${ARCHIVE_NAME}.tar.gz with others"
    echo "   2. Recipients should extract: tar -xzf ${ARCHIVE_NAME}.tar.gz"
    echo "   3. Recipients should build: cd ${PROJECT_NAME} && ./clean_and_build.sh"
    echo ""
    echo "📖 For more details, see PACKAGING_GUIDE.md"
else
    echo "❌ Failed to create archive!"
    exit 1
fi

# Optional: Create a zip version too
read -p "📦 Create a .zip version as well? (y/n) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "📦 Creating ZIP archive..."
    zip -r "${ARCHIVE_NAME}.zip" "${PROJECT_NAME}/" \
        -x "*/build/*" \
        -x "*/CMakeFiles/*" \
        -x "*/CMakeCache.txt" \
        -x "*/cmake_install.cmake" \
        -x "*/Makefile" \
        -x "*.o" \
        -x "*.lock" \
        -x "*/.git/*" \
        -x "*/.gitignore" \
        > /dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        echo "✅ ZIP archive created!"
        echo "   File: ${ARCHIVE_NAME}.zip"
        echo "   Size: $(du -h "${ARCHIVE_NAME}.zip" | cut -f1)"
    else
        echo "⚠️  ZIP creation failed (zip command may not be installed)"
    fi
fi

echo ""
echo "✅ Packaging complete!"

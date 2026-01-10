#!/bin/bash

# Clean and Build Script for MultiPlayer Game
# This script removes all generated build files and rebuilds the project from scratch
# It also checks for and installs missing dependencies on Ubuntu/Debian
# NOTE: This variant prefers GCC/G++-14 packages (gcc-14, g++-14)

# Parse command line arguments
INSTALL_DEPS=true
for arg in "$@"; do
    case $arg in
        --no-install-deps)
            INSTALL_DEPS=false
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --no-install-deps    Skip automatic dependency installation"
            echo "  --help               Show this help message"
            exit 0
            ;;
    esac
done

# Function to check if running on Ubuntu/Debian
is_debian_based() {
    [ -f /etc/debian_version ] || [ -f /etc/lsb-release ]
}

# Function to check if a package is installed
is_package_installed() {
    dpkg -l "$1" 2>/dev/null | grep -q "^ii"
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check and install dependencies
if [ "$INSTALL_DEPS" = true ]; then
    echo "🔍 Checking for required dependencies..."
    echo ""
    
    MISSING_PACKAGES=()
    REQUIRED_PACKAGES=(
        "cmake"
        "gcc-14"
        "g++-14"
        "libgl1-mesa-dev"
        "libx11-dev"
        "libxrandr-dev"
        "libxinerama-dev"
        "libxcursor-dev"
        "libxi-dev"
        "libsqlite3-dev"
        "libssl-dev"
    )
    
    # Check if running on Debian-based system
    if is_debian_based; then
        for package in "${REQUIRED_PACKAGES[@]}"; do
            if ! is_package_installed "$package"; then
                MISSING_PACKAGES+=("$package")
            fi
        done
        
        if [ ${#MISSING_PACKAGES[@]} -gt 0 ]; then
            echo "📦 Missing packages detected: ${MISSING_PACKAGES[*]}"
            echo ""
            echo "Would you like to install them now? This requires sudo privileges."
            read -p "Install missing dependencies? (y/N): " -n 1 -r
            echo ""
            
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                echo "📥 Installing dependencies..."
                sudo apt-get update
                sudo apt-get install -y "${MISSING_PACKAGES[@]}"
                
                if [ $? -ne 0 ]; then
                    echo "❌ Failed to install some dependencies. Please install them manually:"
                    echo "   sudo apt-get install ${MISSING_PACKAGES[*]}"
                    exit 1
                fi
                echo "✅ Dependencies installed successfully!"
                
                # Offer to switch system gcc/g++ to the 14 versions if present
                if command_exists g++-14 && command_exists gcc-14; then
                    echo ""
                    echo "ℹ️  Detected gcc-14 and g++-14 installed."
                    read -p "Would you like to configure system gcc/g++ to point to gcc-14/g++-14 via update-alternatives? (y/N): " -n 1 -r
                    echo ""
                    if [[ $REPLY =~ ^[Yy]$ ]]; then
                        sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 \
                                                --slave /usr/bin/g++ g++ /usr/bin/g++-14
                        sudo update-alternatives --set gcc /usr/bin/gcc-14
                        echo "✅ System gcc/g++ configured to gcc-14/g++-14."
                    else
                        echo "⚠️  Skipping update-alternatives. You can set it later if desired."
                    fi
                fi

            else
                echo "⚠️  Skipping dependency installation. Build may fail if dependencies are missing."
                echo "   To install manually: sudo apt-get install ${MISSING_PACKAGES[*]}"
            fi
        else
            echo "✅ All required dependencies are already installed!"
        fi
    else
        echo "⚠️  Non-Debian system detected. Please install dependencies manually."
        echo "   Required: cmake, gcc-14/g++-14 (or equivalent), OpenGL, X11, SQLite3, OpenSSL development libraries"
    fi
    echo ""
fi

echo "🧹 Cleaning old build files..."

# Remove all CMake generated files
rm -rf build/
rm -rf CMakeFiles/
rm -f CMakeCache.txt
rm -f cmake_install.cmake
rm -f Makefile

echo "✅ Clean complete!"
# echo ""
# echo "🔨 Creating build directory..."

# Create fresh build directory
mkdir -p build
cd build

# echo "⚙️  Running CMake..."

# Generate build files
cmake ..

if [ $? -ne 0 ]; then
    echo "❌ CMake failed! Please check the error messages above."
    exit 1
fi

# echo "✅ CMake configuration complete!"
# echo ""
# echo "🔨 Building project..."

# Build the project
make -j$(nproc)

# if [ $? -ne 0 ]; then
#     echo "❌ Build failed! Please check the error messages above."
#     exit 1
# fi

echo ""
echo "✅ Build complete! Executables are in the build directory."
echo ""
echo "To run the game:"
echo "  cd build"
echo "  ./multyPlayer"

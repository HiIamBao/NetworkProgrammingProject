# Build Instructions for MultiPlayer Game

## ⚠️ Important: First Time Setup

**If you received this project from someone else**, you MUST regenerate the build files on your machine. The build files (Makefile, CMakeCache.txt, etc.) contain hardcoded paths from the original developer's computer and will NOT work on your system.

## Quick Start (Recommended)

### Option 1: Using the Build Script

```bash
# Navigate to the project directory
cd multiPlayerGame

# Run the clean and build script
./clean_and_build.sh
```

This script will:
1. Remove all old build artifacts
2. Create a fresh build directory
3. Run CMake to generate new build files
4. Compile the project

### Option 2: Manual Build

```bash
# Navigate to the project directory
cd multiPlayerGame

# Clean old build files (IMPORTANT!)
rm -rf build/ CMakeFiles/ CMakeCache.txt cmake_install.cmake Makefile

# Create and enter build directory
mkdir build
cd build

# Generate build files
cmake ..

# Build the project
make -j$(nproc)
```

## Running the Game

After building successfully:

```bash
cd build
./multyPlayer
```

## Dependencies

This project requires:
- **CMake** (version 3.10 or higher)
- **C++ Compiler** (g++ or clang with C++17 support)
- **OpenGL development libraries**
- **X11 development libraries** (for Linux)

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install cmake g++ libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

**Fedora:**
```bash
sudo dnf install cmake gcc-c++ mesa-libGL-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
```

**Arch Linux:**
```bash
sudo pacman -S cmake gcc mesa libx11 libxrandr libxinerama libxcursor libxi
```

## Troubleshooting

### Build Fails with "No such file or directory" errors

**Cause:** You're using build files from another computer with hardcoded paths.

**Solution:** Clean and regenerate build files:
```bash
rm -rf build/ CMakeFiles/ CMakeCache.txt cmake_install.cmake Makefile
./clean_and_build.sh
```

### CMake Can't Find Libraries

**Cause:** Missing dependencies.

**Solution:** Install the required dependencies (see Dependencies section above).

### "Permission Denied" when running build script

**Cause:** Script is not executable.

**Solution:**
```bash
chmod +x clean_and_build.sh
./clean_and_build.sh
```

## Project Structure

```
multiPlayerGame/
├── CMakeLists.txt          # Build configuration
├── clean_and_build.sh      # Automated build script
├── src/                    # Source files
├── include/                # Header files
├── resources/              # Game assets (textures, fonts, etc.)
├── thirdparty/            # External libraries (bundled)
└── build/                 # Generated build files (created by CMake)
```

## Development Notes

### Rebuilding After Changes

If you modify source files, you only need to rebuild:
```bash
cd build
make -j$(nproc)
```

### Complete Clean Build

If you encounter weird build issues or change CMakeLists.txt:
```bash
./clean_and_build.sh
```

### Debugging Build

To see detailed compiler commands:
```bash
cd build
make VERBOSE=1
```

## Game Features

### Multi-Room Support
- Hosts can run up to 3 game rooms simultaneously
- Each room runs on a different port (8888, 8889, 8890)

### LAN Discovery
- Automatic room discovery on local network
- No need to manually enter IP addresses

### Game Modes
- **Cooperative** - Work together with other players
- **Free-for-All Deathmatch** - Compete for the highest kills

### Controls
- **WASD** - Movement
- **Mouse** - Aim
- **Left Click** - Shoot
- **ESC** - Pause Menu (in-game)

### Pause Menu
While in a match, press **ESC** to open the pause menu:
- **Resume** - Return to game
- **Leave Match** - Exit to lobby
- **Exit Game** - Quit application

## Documentation

For more detailed documentation, see:
- `FREE_FOR_ALL_DEATHMATCH.md` - Deathmatch mode details
- `DEATHMATCH_QUICK_REF.md` - Quick reference
- `PAUSE_MENU_COMPLETE.md` - Pause menu implementation
- `ESC_KEY_FIX.md` - ESC key handling details

## Support

If you encounter issues:
1. Make sure all dependencies are installed
2. Try a clean rebuild: `./clean_and_build.sh`
3. Check that you're using CMake 3.10 or higher: `cmake --version`
4. Verify your compiler supports C++17: `g++ --version`

## License

[Add your license information here]

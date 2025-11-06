# C++ Multiplayer Game - Build Instructions

## Successfully Built! 🎉

Your C++ multiplayer game project has been successfully built on Linux.

### Built Executables:

1. **multyPlayer** - The main multiplayer game executable
2. **levelBuilder** - Level editor tool for creating game maps

### Location:
- Build directory: `/home/bao/Network Programming/Project/multiPlayerGame/build/`
- Executables are located in the build directory
- Resources have been copied to: `build/resources/`

### How to Run:

#### Main Game:
```bash
cd "/home/bao/Network Programming/Project/multiPlayerGame/build"
./multyPlayer
```

#### Level Builder:
```bash
cd "/home/bao/Network Programming/Project/multiPlayerGame/build"
./levelBuilder
```

### Dependencies Installed:
- CMake 3.28.3
- Build essentials (gcc, g++, make)
- OpenGL development libraries
- GLFW, GLEW, GLM
- X11 development libraries
- Audio libraries (OpenAL, libsndfile)

### Fixes Applied:
1. **Header file case sensitivity**: Fixed `phisics.h` to `Phisics.h` includes
2. **Missing include**: Added `#include <cstring>` to packet.cpp for memcpy
3. **Invalid syntax**: Removed invalid `void;` statement from Phisics.cpp
4. **Platform-specific linking**: Made Windows library linking conditional for ENet

### Project Structure:
- Main game source: `src/gameLayer/`
- Platform code: `src/platform/`
- Common code: `include/common/`
- Third-party libraries: `thirdparty/`

### Networking:
This is a multiplayer game using ENet library for networking. The game supports:
- Server functionality
- Client functionality
- Cross-platform networking (tested with 5 players across Europe according to README)

### Notes:
- The game uses OpenGL for rendering
- ImGui is included for UI
- Physics system is custom implemented
- Game resources are in the `resources/` directory

To make changes and rebuild:
1. Edit source files as needed
2. Run `make` in the build directory to rebuild
3. Run `cmake ..` if you modify CMakeLists.txt files

Enjoy your multiplayer game! 🎮

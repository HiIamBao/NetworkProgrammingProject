# Multiplayer Online Game - Codebase Context

## Overview
This is a **multiplayer online arena shooter** game built with C++ using OpenGL for rendering and ENet for networking. The game features multiple game modes including Deathmatch, Horde Defense, and Boss Fight, with a complete account system, room-based matchmaking, and LAN discovery.

## Project Structure

### Root Directory
```
NetworkProgrammingProject/
├── src/                    # Source code
│   ├── gameLayer/         # Game logic and networking
│   └── platform/          # Platform-specific code (GLFW, input)
├── include/               # Header files
│   ├── gameLayer/         # Game headers
│   ├── platform/          # Platform headers
│   └── common/            # Common utilities
├── resources/             # Game assets (textures, fonts, audio)
├── thirdparty/           # External libraries
├── build/                # Build output
└── CMakeLists.txt        # Build configuration
```

## Core Architecture

### Entry Point Flow
1. **main()** (`src/platform/glfwMain.cpp`)
   - Initializes GLFW window and OpenGL context
   - Sets up input callbacks (keyboard, mouse)
   - Initializes ImGui for debug UI
   - Calls `initGame()` to initialize game systems
   - Main game loop calls `gameLogic(deltaTime)` every frame
   - Handles window management and fullscreen toggling

2. **initGame()** (`src/gameLayer/gameLayer.cpp`)
   - Initializes renderer and loads textures
   - Initializes ENet networking library
   - Creates global managers:
     - `AccountManager` - Database-backed user accounts
     - `SessionManager` - Login session management
     - `AccountUI` - Account system UI
     - `RoomManager` - Game room management
     - `RoomUI` - Room browser UI
     - `LANDiscovery` - UDP broadcast for LAN games
     - `MultiRoomManager` - Multiple concurrent game rooms
     - `AudioManager` - Sound and music playback
   - Initializes glUI library for in-game UI

3. **gameLogic()** (`src/gameLayer/gameLayer.cpp`)
   - Main state machine with 3 states:
     - **State 0**: Main menu / Account system
     - **State 1**: Client (joined game)
     - **State 2**: Host (hosting + playing)
   - Handles UI rendering and state transitions
   - Manages pause menu (ESC key)
   - Detects disconnections and returns to menu

## File Structure & Functions

### Platform Layer (`src/platform/`)

#### `glfwMain.cpp`
- **Purpose**: Application entry point and window management
- **Key Functions**:
  - `main()` - Creates window, initializes systems, runs game loop
  - `keyCallback()` - Processes keyboard input
  - `mouseCallback()` - Processes mouse input
  - `windowFocusCallback()` - Handles window focus changes

#### `platformInput.cpp`
- **Purpose**: Input state management
- **Functions**: Button state tracking, mouse position, typed input

### Game Layer (`src/gameLayer/`)

#### `gameLayer.cpp` ⭐ **MAIN GAME CONTROLLER**
- **Purpose**: Top-level game state management and UI orchestration
- **Key Functions**:
  - `initGame()` - Initialize all subsystems
  - `gameLogic(deltaTime)` - Main state machine (menu/client/host)
  - `closeGame()` - Cleanup all systems
- **State Management**:
  - State 0: Account UI, room browser, main menu
  - State 1: Client mode (connected to server)
  - State 2: Host mode (running server + client)
- **Global Managers Created Here**:
  - `g_accountManager` - Account database
  - `g_sessionManager` - Session tokens
  - `g_accountUI` - Account UI screens
  - `g_roomManager` - Room list management
  - `g_roomUI` - Room browser UI
  - `g_lanDiscovery` - LAN server discovery
  - `g_multiRoomManager` - Multiple room hosting

#### `client.cpp` ⭐ **CLIENT NETWORKING & RENDERING**
- **Purpose**: Client-side game logic, rendering, and network message handling
- **Key Functions**:
  - `clientFunction(deltaTime, renderer, textures, ip, name, port)` - Main client loop
  - `connectToServer()` - Establish connection to game server
  - `msgLoop()` - Process incoming network packets
  - `resetClient()` - Reset client state
  - `sendPlayerData()` - Send player position/state to server
- **Packet Handlers**: Processes all incoming packets (player updates, game events, etc.)
- **Rendering**: Draws game world, players, enemies, UI overlays
- **Game State**: Maintains local copies of:
  - Other players' entities
  - Items on map
  - Enemies (for Horde Defense/Boss Fight)
  - Leaderboard data
  - Match end statistics

#### `server.cpp` ⭐ **SERVER NETWORKING & GAME LOGIC**
- **Purpose**: Server-side game logic and authoritative state management
- **Key Functions**:
  - `serverFunction(port, gameMode, mapId)` - Main server loop
  - `addConnection()` - Handle new player connections
  - `removeConnection()` - Handle player disconnections
  - `recieveData()` - Process incoming client packets
  - `broadCast()` - Send packets to all clients
- **Server Instance**: Each server runs in its own thread with:
  - `ServerInstance` struct containing:
    - Player connections map
    - Items and spawn positions
    - Map data
    - Game mode manager (Horde/Boss Fight)
- **Game Modes**: Delegates to specialized managers:
  - `HordeDefenseManager` for wave-based survival
  - `BossFightManager` for boss encounters

#### `packet.h` & `packet.cpp`
- **Purpose**: Network packet definitions and serialization
- **Packet Types** (70+ packet headers):
  - Authentication: Register, Login, Logout, ForceDisconnect
  - Game: PlayerUpdate, Bullet, Hit, Item, Kill, Death
  - Room System: Create, Join, Leave, RoomList, PlayerReady
  - Horde Defense: WaveStart, EnemySpawn, EnemyUpdate, BuyUpgrade
  - Boss Fight: BossSpawn, BossAttack, BossUpdate, MatchEnd
  - Leaderboard: UpdateLeaderBoard
- **Data Structures**: 
  - `MatchEndData` - Match results with player scores
  - `HordeStateUpdateData` - Wave info, enemies remaining
  - `BossFightStateUpdateData` - Boss health, phase
  - `LeaderBoardUpdateData` - Real-time leaderboard

### Account System

#### `AccountManager.cpp`
- **Purpose**: SQLite database for persistent user accounts
- **Key Functions**:
  - `registerAccount()` - Create new account with hashed password
  - `validateCredentials()` - Check login credentials
  - `updateStats()` - Update player stats after match
  - `getTopPlayers()` - Leaderboard queries
  - `recordDeathmatchMatchEnd()` - Save match results
  - `recordHordeDefenseMatchEnd()` - Save Horde stats
  - `recordBossFightMatchEnd()` - Save Boss Fight stats
- **Database Tables**:
  - `accounts` - User credentials and stats
  - `match_history` - Per-match records
  - Session control (prevent multi-login)

#### `SessionManager.cpp`
- **Purpose**: Manage login sessions with tokens
- **Functions**: Generate session tokens, validate sessions, logout

#### `AccountUI.cpp`
- **Purpose**: UI screens for account system
- **Screens**:
  - Login screen
  - Registration screen
  - Main menu (after login)
  - Leaderboard display
  - Match summary screen
- **Uses glUI library** for rendering buttons, text inputs, tables

### Room System

#### `RoomManager.cpp`
- **Purpose**: Manage game room lifecycle
- **Functions**:
  - `createRoom()` - Create new game room
  - `deleteRoom()` - Remove room
  - `addPlayerToRoom()` - Add player to room
  - `removePlayerFromRoom()` - Remove player
  - `getAvailableRooms()` - List joinable rooms

#### `GameRoom.cpp`
- **Purpose**: Individual room state
- **Data**: Room name, host, players, game mode, map, password, status

#### `RoomUI.cpp`
- **Purpose**: Room browser and lobby UI
- **Screens**:
  - Room browser (list of available rooms)
  - Create room dialog
  - Room lobby (player list, ready status)
  - Joining room loading screen

#### `RoomHandler.cpp`
- **Purpose**: Handle room-related network packets on server

#### `LANDiscovery.cpp`
- **Purpose**: UDP broadcast for LAN game discovery
- **Functions**:
  - `startBroadcasting()` - Advertise server on LAN
  - `startListening()` - Listen for LAN servers
  - `getDiscoveredServers()` - Get list of found servers

#### `MultiRoomManager.cpp`
- **Purpose**: Manage multiple concurrent game servers
- **Functions**:
  - `createRoom()` - Start server on available port
  - `stopRoom()` - Stop specific server
  - `getActiveRooms()` - List running servers

### Game Modes

#### `HordeDefenseManager.cpp`
- **Purpose**: Wave-based survival mode (cooperative)
- **Features**:
  - 20 waves of increasing difficulty
  - Enemy types: Zombie, Runner, Tank, Exploder, Boss
  - Player upgrades: Damage, Fire Rate, Health, Speed
  - Shop items: Speed boost, damage boost, multi-shot
  - Player respawning between waves
- **Key Functions**:
  - `update(deltaTime)` - Main game loop
  - `startWave()` - Begin new wave
  - `spawnEnemy()` - Create enemy
  - `damageEnemy()` - Process bullet hits
  - `buyUpgrade()` - Purchase permanent upgrade
  - `endMatch()` - Calculate MVP and stats

#### `BossFightManager.cpp`
- **Purpose**: Boss battle mode (cooperative)
- **Features**:
  - Single powerful boss enemy
  - Boss phases (health thresholds)
  - Boss attacks: Melee, Circle Spray, AOE
  - Player respawning
  - Damage tracking for MVP
- **Key Functions**:
  - `update(deltaTime)` - Main game loop
  - `spawnBoss()` - Create boss
  - `damageBoss()` - Process bullet hits
  - `executeBossAttack()` - Boss AI
  - `endMatch()` - Victory/defeat with stats

#### `TowerDefense.cpp`
- **Purpose**: Tower defense mode (appears to be legacy/unused)

### UI System

#### `Ui.cpp`
- **Purpose**: Basic UI utilities
- **Functions**: Color definitions, UI helper functions

#### `glui/` (Third-party library)
- **Purpose**: Immediate-mode GUI library for in-game UI
- **Features**: Buttons, text inputs, tables, menus, layouts
- **Used by**: All UI rendering (AccountUI, RoomUI, in-game menus)

### Audio System

#### `AudioManager.cpp`
- **Purpose**: Sound and music playback
- **Functions**:
  - `init()` - Initialize audio device
  - `playClick()` - Button click sound
  - `playMusic()` - Background music
  - `update()` - Stream music updates
- **Thread-safe**: Uses mutex for audio device access

### Authentication

#### `AuthenticationHandler.cpp`
- **Purpose**: Server-side authentication packet handling
- **Functions**: Process login/register/logout requests

## Program Flow

### 1. Application Startup
```
main() 
  → Initialize GLFW window
  → Initialize OpenGL/GLAD
  → Initialize ImGui
  → initGame()
    → Load textures
    → Initialize ENet
    → Create AccountManager (SQLite DB)
    → Create SessionManager
    → Create AccountUI
    → Create RoomManager
    → Create LANDiscovery
    → Create AudioManager
  → Enter main game loop
```

### 2. Main Menu Flow (State 0)
```
gameLogic() [State 0]
  → AccountUI renders login/register screen
  → User logs in
  → AccountUI shows main menu
  → User selects "Browse Rooms"
  → RoomUI renders room browser
  → LANDiscovery listens for LAN servers
  → User clicks "Create Room" or "Join Room"
```

### 3. Create Room Flow
```
User clicks "Create Room"
  → RoomUI shows create room dialog
  → User fills in: name, max players, game mode, map
  → MultiRoomManager.createRoom()
    → Starts serverFunction() in new thread on available port
    → LANDiscovery.startBroadcasting() advertises room
  → gameLayer.cpp transitions to State 2 (host mode)
  → connectToServer("127.0.0.1", port)
  → clientFunction() starts rendering game
```

### 4. Join Room Flow
```
User clicks "Join Room"
  → LANDiscovery provides list of discovered servers
  → User selects room
  → gameLayer.cpp transitions to State 1 (client mode)
  → connectToServer(serverIP, port)
  → clientFunction() starts rendering game
```

### 5. In-Game Flow (State 1/2)
```
clientFunction(deltaTime)
  → msgLoop() - Process incoming packets
    → Update other players' positions
    → Update enemies (Horde/Boss modes)
    → Update leaderboard
    → Handle match end
  → Render game world
    → Draw map tiles
    → Draw players
    → Draw enemies
    → Draw bullets
    → Draw UI overlays (health, ammo, leaderboard)
  → Handle input
    → WASD movement
    → Mouse aim
    → Click to shoot
    → ESC to pause
  → sendPlayerData() - Send position to server
```

### 6. Server Game Loop
```
serverFunction(port, gameMode, mapId)
  → Create ServerInstance
  → Initialize game mode manager (Horde/Boss)
  → Main loop:
    → Poll ENet events
      → addConnection() - New player joins
      → recieveData() - Process client packets
      → removeConnection() - Player leaves
    → Update game mode
      → HordeDefenseManager.update()
        → Spawn enemies
        → Update enemy AI
        → Check wave completion
        → Broadcast state updates
      → BossFightManager.update()
        → Update boss AI
        → Process attacks
        → Check victory/defeat
    → Broadcast player updates
    → Broadcast enemy updates (10Hz)
```

### 7. UI to In-Game Rendering Transition
```
State 0 (Menu) → State 1/2 (In-Game)
  ↓
gameLogic() detects state change
  ↓
Stops rendering AccountUI/RoomUI
  ↓
Calls clientFunction(deltaTime, renderer, textures, ip, name, port)
  ↓
clientFunction() takes over rendering:
  - Clears screen
  - Renders game world (tiles, entities)
  - Renders game-specific UI (health bars, leaderboard)
  - Handles game input (movement, shooting)
```

### 8. Match End Flow
```
Server detects match end condition
  → Calculates final stats (kills, damage, MVP)
  → Sends MatchEndData packet to all clients
  → Client receives packet
    → Saves match to AccountManager database
    → AccountUI.setState(MATCH_SUMMARY)
    → gameLayer.cpp detects state change
    → Transitions back to State 0
    → AccountUI renders match summary screen
      → Shows MVP, player stats table
      → "Return to Menu" button
```

### 9. Pause Menu Flow
```
User presses ESC during game (State 1/2)
  → gameLayer.cpp sets isPaused = true
  → clientFunction(0, ...) - Render frozen game
  → glUI renders pause menu overlay:
    - "Resume Game"
    - "Leave Match"
    - "Exit Game"
  → User clicks "Leave Match"
    → closeFunction() - Disconnect from server
    → resetClient() - Clear client state
    → Stop LANDiscovery broadcasting (if host)
    → closeServerByPort() (if host)
    → Transition to State 0
```

## Key Design Patterns

### 1. **Client-Server Architecture**
- **Server is authoritative**: All game logic runs on server
- **Client is predictive**: Renders local player immediately, waits for server confirmation
- **ENet networking**: Reliable and unreliable channels

### 2. **State Machine**
- `gameLayer.cpp` uses simple integer state (0=menu, 1=client, 2=host)
- Each state has different rendering and input handling

### 3. **Manager Pattern**
- Specialized managers for each subsystem:
  - `AccountManager` - Database operations
  - `RoomManager` - Room lifecycle
  - `HordeDefenseManager` - Horde mode logic
  - `BossFightManager` - Boss mode logic
  - `AudioManager` - Sound playback

### 4. **Packet-Based Networking**
- All network communication uses typed packets
- `packet.h` defines 70+ packet types
- Each packet has header (type) and data payload

### 5. **Immediate Mode UI**
- Uses glUI library (similar to Dear ImGui)
- UI code runs every frame
- No retained UI state

## Important Global Variables

### In `gameLayer.cpp`:
- `g_accountManager` - Account database
- `g_sessionManager` - Session tokens
- `g_accountUI` - Account UI renderer
- `g_roomManager` - Room list
- `g_roomUI` - Room browser UI
- `g_lanDiscovery` - LAN discovery
- `g_multiRoomManager` - Multi-room hosting

### In `client.cpp`:
- `server` (ENetPeer*) - Connection to server
- `cid` (int32_t) - Client ID assigned by server
- `joined` (bool) - Connection status
- `otherPlayers` - Map of other players
- `items` - Items on map
- `activeLeaderboard` - Real-time leaderboard data

### In `server.cpp`:
- `serverInstances` - Map of port → ServerInstance
- Each `ServerInstance` contains:
  - `connections` - Map of cid → Client
  - `items` - Items on map
  - `hordeDefenseManager` - Horde mode state
  - `bossFightManager` - Boss mode state

## Threading Model

- **Main Thread**: Rendering and UI (GLFW/OpenGL)
- **Server Threads**: Each server runs in separate thread (via `std::thread`)
- **Audio Thread**: Background music streaming
- **LAN Discovery Thread**: UDP broadcast/listen

## Database Schema

### `accounts` table:
- `username` (PRIMARY KEY)
- `password_hash`
- `email`
- `level`
- `experience`
- `wins`, `losses`
- `kills`, `deaths`
- `is_logged_in` (session control)
- Game mode specific stats (horde_waves, boss_damage, etc.)

### `match_history` table:
- `id` (PRIMARY KEY)
- `username` (FOREIGN KEY)
- `game_mode`
- `result` (win/loss)
- `score`
- `timestamp`
- `extra_data` (JSON)

## Network Protocol

### Connection Flow:
1. Client sends connection request to server
2. Server assigns CID (Client ID)
3. Server sends `headerReceiveCIDAndData` with CID and initial state
4. Server broadcasts `headerAnounceConnection` to other clients
5. Client receives other players' data
6. Client is now "joined"

### Packet Channels:
- **Channel 0**: Reliable ordered (game events, state changes)
- **Channel 1**: Unreliable (player positions, enemy updates)

### Update Rates:
- Player position: Every frame (unreliable)
- Enemy updates: 10 Hz (unreliable, batched)
- Game state: On change (reliable)

## Common Workflows

### Adding a New Game Mode:
1. Create manager class (e.g., `NewModeManager.cpp`)
2. Add packet types to `packet.h`
3. Add game mode enum to `GameRoom.h`
4. Update `server.cpp` to instantiate manager
5. Update `client.cpp` to handle new packets
6. Add UI in `RoomUI.cpp` for mode selection

### Adding a New Packet Type:
1. Add header enum to `packet.h`
2. Define data structure in `packet.h`
3. Add handler in `server.cpp` `recieveData()`
4. Add handler in `client.cpp` `msgLoop()`
5. Use `sendPacket()` to send, `parsePacket()` to receive

### Adding a New UI Screen:
1. Add state enum to `AccountUI.h` or `RoomUI.h`
2. Implement render function (e.g., `renderNewScreen()`)
3. Add state transition logic
4. Use glUI functions for buttons, text, inputs

## Build System

- **CMake**: Build configuration in `CMakeLists.txt`
- **Dependencies**: ENet, OpenGL, GLFW, SQLite, OpenSSL, stb libraries
- **Build Script**: `clean_and_build.sh` for Linux

## Resources

- **Textures**: `resources/*.png` (sprites, UI backgrounds)
- **Fonts**: `resources/font/ANDYB.TTF`
- **Audio**: `resources/*.ogg` (music, sound effects)
- **Maps**: `resources/maps/*.map` (tile-based level data)

## Summary

This is a **feature-rich multiplayer game** with:
- ✅ Account system with persistent stats
- ✅ Room-based matchmaking
- ✅ LAN discovery
- ✅ Multiple game modes (Deathmatch, Horde, Boss Fight)
- ✅ Real-time leaderboards
- ✅ Match history and statistics
- ✅ Audio system
- ✅ Pause menu and match summaries

The architecture cleanly separates:
- **Platform layer** (GLFW, input) from **game layer** (logic, networking)
- **Client rendering** from **server authority**
- **UI system** (glUI) from **game rendering** (gl2d)
- **Account management** from **game logic**

The codebase is well-structured for a multiplayer game, with clear separation of concerns and modular game mode managers.

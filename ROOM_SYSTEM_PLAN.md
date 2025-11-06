# Room System Implementation Plan

## 🎯 Objective
Add a complete room/lobby system to the multiplayer game with UI integration, allowing players to create, browse, and join game rooms.

## 📊 Requirements Coverage
This implementation will achieve:
- **Room Creation** (1-2 points): Create game rooms with settings
- **Room Management** (2 points): List, join, leave rooms
- **Enhanced Multiplayer** (3-5 points): Organized game sessions
- **UI Enhancement** (2-3 points): Room browsing and management interface

**Total Points**: 8-12 points

---

## 🏗️ Architecture Overview

### Components to Create:

#### 1. Backend (Server-Side)
- `RoomManager` - Manages all game rooms
- `GameRoom` - Individual room data structure
- `RoomHandler` - Handles room-related packets

#### 2. Frontend (Client-Side)
- `RoomUI` - UI for room browsing, creation, joining
- Room list display with filters
- Room creation dialog
- Room details view

#### 3. Protocol
- New packet types for room operations
- Room data structures for network transmission

---

## 📋 Detailed Implementation Plan

### Phase 1: Backend - Room Data Structures (30 minutes)

#### 1.1 Create `GameRoom` class
**File**: `include/gameLayer/GameRoom.h`

Features:
- Room ID (unique identifier)
- Room name
- Host username
- Current players list (username + peer)
- Max players (2-8)
- Game mode (deathmatch, team, coop)
- Map selection
- Room status (waiting, in_game, finished)
- Password protection (optional)
- Created timestamp

#### 1.2 Create `RoomManager` class
**File**: `include/gameLayer/RoomManager.h`

Features:
- Create room
- Delete room
- Find room by ID
- Get all rooms / available rooms
- Add player to room
- Remove player from room
- Update room status
- Get room count
- Validate room capacity

---

### Phase 2: Backend - Room Handler (30 minutes)

#### 2.1 Create `RoomHandler` class
**File**: `include/gameLayer/RoomHandler.h`

Handles packets:
- `CREATE_ROOM_REQUEST`
- `JOIN_ROOM_REQUEST`
- `LEAVE_ROOM_REQUEST`
- `GET_ROOM_LIST_REQUEST`
- `GET_ROOM_INFO_REQUEST`
- `START_GAME_REQUEST` (host only)
- `KICK_PLAYER_REQUEST` (host only)

---

### Phase 3: Protocol Definition (15 minutes)

#### 3.1 Update `packet.h`

Add new packet types:
```cpp
headerCreateRoomRequest,
headerCreateRoomResponse,
headerJoinRoomRequest,
headerJoinRoomResponse,
headerLeaveRoomRequest,
headerLeaveRoomResponse,
headerGetRoomListRequest,
headerGetRoomListResponse,
headerGetRoomInfoRequest,
headerGetRoomInfoResponse,
headerStartGameRequest,
headerStartGameResponse,
headerRoomPlayerJoined,    // Broadcast
headerRoomPlayerLeft,       // Broadcast
headerRoomStatusChanged,    // Broadcast
```

Add data structures:
```cpp
struct CreateRoomData {
    char roomName[32];
    char password[32];
    int maxPlayers;
    int gameMode;
    int mapId;
};

struct RoomInfoData {
    int roomId;
    char roomName[32];
    char hostUsername[32];
    int currentPlayers;
    int maxPlayers;
    int gameMode;
    int mapId;
    bool hasPassword;
    bool isInGame;
};

struct JoinRoomData {
    int roomId;
    char password[32];
};
```

---

### Phase 4: Frontend - Room UI (45 minutes)

#### 4.1 Create `RoomUI` class
**File**: `include/gameLayer/RoomUI.h`

UI States:
- Room Browser (list of rooms)
- Create Room Dialog
- Room Lobby (inside a room)
- Room Settings (host only)

Features:
- Display room list with sorting/filtering
- Create room button with settings dialog
- Join room button
- Password input dialog
- Room lobby showing players
- Ready/Not Ready indicators
- Chat in room (optional)
- Start game button (host only)
- Leave room button

---

### Phase 5: Integration (30 minutes)

#### 5.1 Server Integration
- Initialize RoomManager in server
- Add RoomHandler to packet processing
- Link with SessionManager for authentication
- Handle player disconnects (remove from rooms)

#### 5.2 Client Integration
- Add RoomUI to main menu flow
- Connect UI buttons to packet sending
- Handle room-related responses
- Update game state based on room status

#### 5.3 Game Flow Updates
- Main Menu → Login → Room Browser
- Room Browser → Create Room → Room Lobby
- Room Browser → Join Room → Room Lobby
- Room Lobby → Start Game → In Game
- In Game → End Game → Room Lobby

---

## 📁 File Structure

### New Files to Create:
```
include/gameLayer/
├── GameRoom.h
├── RoomManager.h
├── RoomHandler.h
└── RoomUI.h

src/gameLayer/
├── GameRoom.cpp
├── RoomManager.cpp
├── RoomHandler.cpp
└── RoomUI.cpp
```

### Files to Modify:
```
include/gameLayer/packet.h          # Add room packet types
src/gameLayer/gameLayer.cpp         # Integrate RoomUI
src/gameLayer/server.cpp            # Add RoomManager/Handler
src/gameLayer/client.cpp            # Handle room responses
```

---

## 🎨 UI Design Mockup

### Room Browser Screen:
```
===== GAME ROOMS =====

[Create Room]  [Refresh]  [Back]

Room List:
┌────────────────────────────────────────────┐
│ 🏠 Epic Battle         👤 4/8   🎮 DeathM  │
│    Host: Player1       🔓 No Pass  ⏸️ Wait │
├────────────────────────────────────────────┤
│ 🏠 Team Warriors       👤 6/6   🎮 Team    │
│    Host: ProGamer      🔒 Pass    ▶️ Play  │
├────────────────────────────────────────────┤
│ 🏠 Chill Game          👤 2/4   🎮 Coop    │
│    Host: ChillDude     🔓 No Pass  ⏸️ Wait │
└────────────────────────────────────────────┘

Filter: [All] [Available] [Has Space] [No Pass]
Sort: [Players] [Name] [Mode] [Status]
```

### Create Room Dialog:
```
===== CREATE ROOM =====

Room Name: [________________]
Password:  [________________] (Optional)

Max Players: [●2  ○4  ○6  ○8]

Game Mode: 
  ○ Deathmatch (Free For All)
  ○ Team Battle (2 Teams)
  ● Cooperative (VS AI)

Map: [▼ Desert Arena    ▼]

[Create]  [Cancel]
```

### Room Lobby Screen:
```
===== EPIC BATTLE =====
Host: Player1  |  Mode: Deathmatch  |  Map: Desert Arena

Players (4/8):
┌────────────────────────────────┐
│ 👑 Player1        ✓ Ready      │
│ 👤 Player2        ✓ Ready      │
│ 👤 Guest123       ⏸️ Not Ready  │
│ 👤 ProGamer       ✓ Ready      │
│ -- Empty --                     │
│ -- Empty --                     │
│ -- Empty --                     │
│ -- Empty --                     │
└────────────────────────────────┘

Chat:
[Player1]: Let's go!
[ProGamer]: Ready!

[Ready] [Leave Room]  |  [Start Game] (Host Only)
```

---

## 🔧 Implementation Steps

### Step 1: Create Backend Data Structures
1. Implement `GameRoom` class
2. Implement `RoomManager` class
3. Add unit tests for room operations

### Step 2: Create Protocol
1. Update `packet.h` with room packet types
2. Define room data structures
3. Test packet serialization

### Step 3: Create Room Handler
1. Implement `RoomHandler` class
2. Add packet processing logic
3. Integrate with SessionManager

### Step 4: Create UI Components
1. Implement `RoomUI` class
2. Create room browser layout
3. Create room creation dialog
4. Create room lobby view

### Step 5: Integration
1. Add RoomManager to server
2. Add RoomHandler to packet processing
3. Add RoomUI to client
4. Test end-to-end flow

### Step 6: Testing & Polish
1. Test room creation
2. Test joining/leaving
3. Test disconnect handling
4. Test game start from room
5. Add error handling
6. Polish UI animations

---

## 🧪 Testing Checklist

### Room Creation:
- [ ] Create room with valid data
- [ ] Create room with password
- [ ] Create room with max player limits
- [ ] Handle duplicate room names
- [ ] Validate input fields

### Room Joining:
- [ ] Join public room
- [ ] Join password-protected room
- [ ] Handle wrong password
- [ ] Handle full room
- [ ] Handle room not found
- [ ] Handle already in room

### Room Management:
- [ ] List all rooms
- [ ] Filter rooms by criteria
- [ ] Sort rooms by attributes
- [ ] Update room list on changes
- [ ] Handle room deletion

### Room Lobby:
- [ ] Display all players
- [ ] Show host indicator
- [ ] Ready/Not Ready toggle
- [ ] Start game (host only)
- [ ] Kick player (host only)
- [ ] Leave room

### Edge Cases:
- [ ] Host leaves room (transfer or close)
- [ ] Player disconnect (remove from room)
- [ ] Room timeout (no activity)
- [ ] Server restart recovery
- [ ] Concurrent operations

---

## 🎯 Success Criteria

1. ✅ Players can create rooms with custom settings
2. ✅ Players can browse available rooms
3. ✅ Players can join rooms (with password support)
4. ✅ Room lobby shows all players and their status
5. ✅ Host can start the game
6. ✅ Room system handles disconnects gracefully
7. ✅ UI is intuitive and responsive
8. ✅ All operations are authenticated (require login)

---

## 📈 Future Enhancements (Optional)

1. **Room Templates**: Save favorite room settings
2. **Friends Only**: Private rooms for friend lists
3. **Spectator Mode**: Watch games in progress
4. **Room Chat**: Text chat in lobby
5. **Room History**: Track past games
6. **Auto-Match**: Quick join best available room
7. **Custom Maps**: User-uploaded maps
8. **Tournament Mode**: Bracket-style competitions
9. **Room Permissions**: Advanced host controls
10. **Room Analytics**: Statistics and insights

---

## 🚀 Ready to Implement?

Once you approve this plan, I'll implement each phase step by step, creating files, adding code, testing, and ensuring everything integrates smoothly with your existing account system.

**Estimated Total Time**: 2-3 hours
**Difficulty**: Medium
**Points Gained**: 8-12 points

Should we proceed with the implementation?

# Room UI Network Integration Guide

## Overview
This guide shows how to connect the RoomUI to the network layer to enable full room system functionality.

## Phase 5: Connect UI to Network

### Step 1: Setup RoomUI Callbacks in gameLayer.cpp

Add this code in `gameLogic()` when initializing the room browser state:

```cpp
// In gameLogic() function, inside the BROWSE_ROOMS state handler
if (uiState == UIState::BROWSE_ROOMS && g_accountUI->getIsLoggedIn()) {
    // Set player name and username for room system
    strncpy(name, g_accountUI->getCurrentUsername().c_str(), playerNameSize - 1);
    name[playerNameSize - 1] = '\0';
    
    // Initialize room UI with current username (if not already done)
    static bool roomUIInitialized = false;
    if (!roomUIInitialized && g_roomUI) {
        g_roomUI->setUsername(g_accountUI->getCurrentUsername());
        
        // Setup network callbacks
        g_roomUI->onRequestRoomList = []() {
            if (server) {
                Packet p;
                p.cid = cid;
                p.header = headerGetRoomListRequest;
                sendPacket(server, p, nullptr, 0, true, 0);
            }
        };
        
        g_roomUI->onCreateRoom = [](const CreateRoomData& data) {
            if (server) {
                Packet p;
                p.cid = cid;
                p.header = headerCreateRoomRequest;
                sendPacket(server, p, (const char*)&data, sizeof(data), true, 0);
            }
        };
        
        g_roomUI->onJoinRoom = [](const JoinRoomData& data) {
            if (server) {
                Packet p;
                p.cid = cid;
                p.header = headerJoinRoomRequest;
                sendPacket(server, p, (const char*)&data, sizeof(data), true, 0);
            }
        };
        
        g_roomUI->onLeaveRoom = []() {
            if (server) {
                Packet p;
                p.cid = cid;
                p.header = headerLeaveRoomRequest;
                sendPacket(server, p, nullptr, 0, true, 0);
            }
        };
        
        g_roomUI->onSetReady = [](bool ready) {
            if (server) {
                SetReadyData data;
                data.ready = ready;
                Packet p;
                p.cid = cid;
                p.header = headerSetReadyRequest;
                sendPacket(server, p, (const char*)&data, sizeof(data), true, 0);
            }
        };
        
        g_roomUI->onStartGame = []() {
            if (server) {
                Packet p;
                p.cid = cid;
                p.header = headerStartGameRequest;
                sendPacket(server, p, nullptr, 0, true, 0);
            }
        };
        
        g_roomUI->onRequestRoomInfo = [](int roomId) {
            if (server) {
                GetRoomInfoData data;
                data.roomId = roomId;
                Packet p;
                p.cid = cid;
                p.header = headerGetRoomInfoRequest;
                sendPacket(server, p, (const char*)&data, sizeof(data), true, 0);
            }
        };
        
        roomUIInitialized = true;
    }
    
    g_roomUI->setState(RoomUIState::ROOM_BROWSER);
    
    // Render room UI (this overrides the account UI temporarily)
    g_roomUI->render(renderer, textures.font, deltaTime);
    
    // Check if user exited room browser
    if (g_roomUI->getState() == RoomUIState::NONE) {
        g_accountUI->setState(UIState::MAIN_MENU);
        roomUIInitialized = false;  // Reset for next time
    }
}
```

### Step 2: Handle Room Packets in client.cpp

Add these cases to the `msgLoop()` function in the `ENET_EVENT_TYPE_RECEIVE` switch statement:

```cpp
// In client.cpp, msgLoop() function, inside ENET_EVENT_TYPE_RECEIVE case

else if (p.header == headerCreateRoomResponse) {
    CreateRoomResponse response = *(CreateRoomResponse*)data;
    if (g_roomUI) {
        g_roomUI->handleCreateRoomResponse(response);
    }
}
else if (p.header == headerJoinRoomResponse) {
    JoinRoomResponse response = *(JoinRoomResponse*)data;
    if (g_roomUI) {
        g_roomUI->handleJoinRoomResponse(response);
    }
}
else if (p.header == headerGetRoomListResponse) {
    RoomListResponse* listResp = (RoomListResponse*)data;
    std::vector<RoomInfoData> rooms;
    
    // Parse room list
    char* ptr = (char*)data + sizeof(RoomListResponse);
    for (int i = 0; i < listResp->roomCount; i++) {
        RoomInfoData* roomData = (RoomInfoData*)ptr;
        rooms.push_back(*roomData);
        ptr += sizeof(RoomInfoData);
    }
    
    if (g_roomUI) {
        g_roomUI->handleRoomListResponse(rooms);
    }
}
else if (p.header == headerGetRoomInfoResponse) {
    DetailedRoomInfo* detailedInfo = (DetailedRoomInfo*)data;
    std::vector<PlayerInRoomData> players;
    
    // Parse player list
    char* ptr = (char*)data + sizeof(DetailedRoomInfo);
    for (int i = 0; i < detailedInfo->playerCount; i++) {
        PlayerInRoomData* playerData = (PlayerInRoomData*)ptr;
        players.push_back(*playerData);
        ptr += sizeof(PlayerInRoomData);
    }
    
    if (g_roomUI) {
        g_roomUI->handleRoomInfoResponse(detailedInfo->info, players);
    }
}
else if (p.header == headerPlayerJoined) {
    PlayerJoinedData joinData = *(PlayerJoinedData*)data;
    if (g_roomUI) {
        g_roomUI->handlePlayerJoined(joinData);
    }
}
else if (p.header == headerPlayerLeft) {
    PlayerLeftData leftData = *(PlayerLeftData*)data;
    if (g_roomUI) {
        g_roomUI->handlePlayerLeft(leftData);
    }
}
else if (p.header == headerPlayerReadyChanged) {
    PlayerReadyChangedData readyData = *(PlayerReadyChangedData*)data;
    if (g_roomUI) {
        g_roomUI->handlePlayerReadyChanged(readyData);
    }
}
else if (p.header == headerRoomStatusChanged) {
    RoomStatusChangedData statusData = *(RoomStatusChangedData*)data;
    if (g_roomUI) {
        g_roomUI->handleRoomStatusChanged(statusData);
    }
}
else if (p.header == headerLeaveRoomResponse) {
    if (g_roomUI) {
        g_roomUI->handleLeaveRoomResponse();
    }
}
```

### Step 3: Declare g_roomUI as extern in client.cpp

At the top of `client.cpp`, add:

```cpp
#include "RoomUI.h"

// External reference to room UI (defined in gameLayer.cpp)
extern RoomUI* g_roomUI;
```

### Step 4: Integrate RoomManager into server.cpp

In `serverFunction()`:

```cpp
// Add at the beginning of serverFunction()
RoomManager roomManager;
AccountManager accountManager;
SessionManager sessionManager(&accountManager);
RoomHandler roomHandler(&accountManager, &sessionManager, &roomManager);

// ... existing code ...

// In the packet processing loop, add:
else if (p.header == headerCreateRoomRequest) {
    CreateRoomData reqData = *(CreateRoomData*)data;
    auto response = roomHandler.handleCreateRoom(reqData, event.peer);
    sendPacket(event.peer, p, (const char*)&response, sizeof(response), true, 0);
}
else if (p.header == headerJoinRoomRequest) {
    JoinRoomData reqData = *(JoinRoomData*)data;
    auto response = roomHandler.handleJoinRoom(reqData, event.peer);
    sendPacket(event.peer, p, (const char*)&response, sizeof(response), true, 0);
}
else if (p.header == headerLeaveRoomRequest) {
    roomHandler.handleLeaveRoom(event.peer);
    // Send confirmation
    Packet resp;
    resp.cid = p.cid;
    resp.header = headerLeaveRoomResponse;
    sendPacket(event.peer, resp, nullptr, 0, true, 0);
}
else if (p.header == headerGetRoomListRequest) {
    auto rooms = roomHandler.handleGetRoomList();
    
    // Calculate total size
    size_t totalSize = sizeof(RoomListResponse) + rooms.size() * sizeof(RoomInfoData);
    char* buffer = new char[totalSize];
    
    RoomListResponse* listResp = (RoomListResponse*)buffer;
    listResp->roomCount = rooms.size();
    
    // Copy room data
    char* ptr = buffer + sizeof(RoomListResponse);
    for (const auto& room : rooms) {
        memcpy(ptr, &room, sizeof(RoomInfoData));
        ptr += sizeof(RoomInfoData);
    }
    
    Packet resp;
    resp.cid = p.cid;
    resp.header = headerGetRoomListResponse;
    sendPacket(event.peer, resp, buffer, totalSize, true, 0);
    
    delete[] buffer;
}
else if (p.header == headerGetRoomInfoRequest) {
    GetRoomInfoData reqData = *(GetRoomInfoData*)data;
    auto [info, players] = roomHandler.handleGetRoomInfo(reqData.roomId);
    
    // Calculate total size
    size_t totalSize = sizeof(DetailedRoomInfo) + players.size() * sizeof(PlayerInRoomData);
    char* buffer = new char[totalSize];
    
    DetailedRoomInfo* detailedInfo = (DetailedRoomInfo*)buffer;
    detailedInfo->info = info;
    detailedInfo->playerCount = players.size();
    
    // Copy player data
    char* ptr = buffer + sizeof(DetailedRoomInfo);
    for (const auto& player : players) {
        memcpy(ptr, &player, sizeof(PlayerInRoomData));
        ptr += sizeof(PlayerInRoomData);
    }
    
    Packet resp;
    resp.cid = p.cid;
    resp.header = headerGetRoomInfoResponse;
    sendPacket(event.peer, resp, buffer, totalSize, true, 0);
    
    delete[] buffer;
}
else if (p.header == headerSetReadyRequest) {
    SetReadyData reqData = *(SetReadyData*)data;
    roomHandler.handleSetReady(event.peer, reqData.ready);
}
else if (p.header == headerStartGameRequest) {
    roomHandler.handleStartGame(event.peer);
}

// Handle disconnects - auto-leave rooms
// In ENET_EVENT_TYPE_DISCONNECT case:
roomHandler.handleLeaveRoom(event.peer);
```

### Step 5: Broadcast Functions in RoomHandler

The RoomHandler already has broadcast functions. Make sure they're being called at the right times:

- `broadcastPlayerJoined()` - After successful join
- `broadcastPlayerLeft()` - After player leaves
- `broadcastPlayerReadyChanged()` - After ready status change
- `broadcastRoomStatusChanged()` - When game starts/ends

### Step 6: Game Start Transition

When the host clicks "Start Game" and the room status changes to IN_GAME:

```cpp
// In client.cpp, when handling headerRoomStatusChanged
if (statusData.newStatus == 1) { // IN_GAME
    // Transition to game state
    state = 1; // or 2 if hosting
    
    // The room UI will show "Game starting..." message
    // The actual game will take over rendering
}
```

## Testing Checklist

1. **Room Creation**
   - [ ] Create room without password
   - [ ] Create room with password
   - [ ] Try different max player limits
   - [ ] Try different game modes
   - [ ] Verify room appears in browser

2. **Room Browsing**
   - [ ] See all available rooms
   - [ ] Auto-refresh works
   - [ ] Manual refresh works
   - [ ] Room info displays correctly

3. **Joining Rooms**
   - [ ] Join public room
   - [ ] Join password-protected room
   - [ ] Wrong password rejection
   - [ ] Full room rejection
   - [ ] In-game room rejection

4. **Room Lobby**
   - [ ] See all players
   - [ ] Host indicator works
   - [ ] Ready/Unready toggle works
   - [ ] See other players' ready status
   - [ ] Host can start when all ready
   - [ ] Host can't start if not all ready
   - [ ] Need 2+ players to start

5. **Leaving Rooms**
   - [ ] Player can leave room
   - [ ] Returns to room browser
   - [ ] Other players notified
   - [ ] Host transfer (if implemented)

6. **Disconnects**
   - [ ] Player disconnect removes from room
   - [ ] Other players notified
   - [ ] Room closed if host disconnects (or host transferred)

7. **Game Start**
   - [ ] Game starts for all players
   - [ ] Room status updates
   - [ ] Players enter game state

## Common Issues and Solutions

### Issue: Callbacks not firing
**Solution**: Make sure `roomUIInitialized` flag is set and callbacks are assigned before entering BROWSE_ROOMS state.

### Issue: Packet data corruption
**Solution**: Use proper size calculations and memcpy for variable-length packets (room lists, player lists).

### Issue: UI not updating
**Solution**: Call the handler functions from `msgLoop()` when packets are received.

### Issue: Server crash on room operations
**Solution**: Verify RoomManager and RoomHandler are properly initialized in `serverFunction()`.

### Issue: Memory leaks
**Solution**: Always `delete[] buffer` after sending variable-length packets.

## Performance Considerations

- Room list auto-refreshes every 5 seconds (configurable in RoomUI.cpp)
- Consider adding rate limiting for room creation
- Limit max number of rooms per player
- Consider timeout for inactive rooms
- Broadcast only to players in the same room when possible

---

**Next Phase**: Full end-to-end testing and polish!

# Room Handler Integration Guide

## 🎯 Phase 3 Complete: Room Handler

The RoomHandler is now fully implemented and ready to be integrated into your server!

---

## 📋 Server Integration Steps

### Step 1: Add RoomHandler to Server

In your server initialization (e.g., `server.cpp` or `gameLayer.cpp`):

```cpp
#include "RoomManager.h"
#include "RoomHandler.h"

// Global instances
AccountManager* g_accountManager = nullptr;
SessionManager* g_sessionManager = nullptr;
RoomManager* g_roomManager = nullptr;
RoomHandler* g_roomHandler = nullptr;

void serverFunction() {
    // ... existing ENet initialization ...
    
    // Initialize room system (after account/session managers)
    g_roomManager = new RoomManager();
    g_roomHandler = new RoomHandler(g_accountManager, g_sessionManager, g_roomManager);
    
    std::cout << "Room system initialized" << std::endl;
    
    // ... rest of server code ...
}
```

---

### Step 2: Add Packet Handling

In your packet handling function:

```cpp
void handlePacket(ENetEvent& event) {
    Packet packet;
    size_t dataSize;
    char* data = parsePacket(event, packet, dataSize);
    
    switch (packet.header) {
        // ... existing authentication packets ...
        
        // Room system packets
        case headerCreateRoomRequest:
            g_roomHandler->handleCreateRoom(event.peer, data, dataSize);
            break;
            
        case headerJoinRoomRequest:
            g_roomHandler->handleJoinRoom(event.peer, data, dataSize);
            break;
            
        case headerLeaveRoomRequest:
            g_roomHandler->handleLeaveRoom(event.peer);
            break;
            
        case headerGetRoomListRequest:
            g_roomHandler->handleGetRoomList(event.peer);
            break;
            
        case headerGetRoomInfoRequest:
            g_roomHandler->handleGetRoomInfo(event.peer, data, dataSize);
            break;
            
        case headerStartGameRequest:
            g_roomHandler->handleStartGame(event.peer);
            break;
            
        case headerSetReadyRequest:
            g_roomHandler->handleSetReady(event.peer, data, dataSize);
            break;
            
        default:
            // ... other packets ...
            break;
    }
}
```

---

### Step 3: Handle Disconnections

In your disconnect event handler:

```cpp
case ENET_EVENT_TYPE_DISCONNECT:
    std::cout << "Client disconnected" << std::endl;
    
    // Handle authentication disconnect
    if (g_authHandler) {
        g_authHandler->handlePeerDisconnect(event.peer);
    }
    
    // Handle room disconnect
    if (g_roomHandler) {
        g_roomHandler->handlePlayerDisconnect(event.peer);
    }
    break;
```

---

### Step 4: Add Periodic Cleanup (Optional but Recommended)

In your main server loop:

```cpp
// Main server loop
while (running) {
    ENetEvent event;
    while (enet_host_service(server, &event, 100) > 0) {
        // ... handle events ...
    }
    
    // Periodic cleanup (every 60 seconds)
    static auto lastCleanup = std::chrono::system_clock::now();
    auto now = std::chrono::system_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCleanup).count() > 60) {
        if (g_sessionManager) {
            g_sessionManager->cleanExpiredSessions();
        }
        if (g_roomHandler) {
            g_roomHandler->cleanupInactiveRooms();
        }
        lastCleanup = now;
    }
}
```

---

### Step 5: Cleanup on Shutdown

```cpp
// Cleanup
enet_host_destroy(server);

delete g_roomHandler;
delete g_roomManager;
delete g_authHandler;
delete g_sessionManager;
delete g_accountManager;

enet_deinitialize();
```

---

## 🧪 Testing the Room Handler

### Test 1: Create Room
1. Login to server
2. Send `headerCreateRoomRequest` with room data
3. Should receive `headerCreateRoomResponse` with success and room ID

### Test 2: Join Room
1. Login with second client
2. Send `headerGetRoomListRequest`
3. Receive list of available rooms
4. Send `headerJoinRoomRequest` with room ID
5. Both clients should receive `headerRoomPlayerJoined`

### Test 3: Ready Up
1. From inside room, send `headerSetReadyRequest` with ready=true
2. All players should receive `headerRoomPlayerReadyChanged`

### Test 4: Start Game
1. All players set ready
2. Host sends `headerStartGameRequest`
3. All players should receive `headerRoomStatusChanged` with status=IN_GAME

### Test 5: Leave Room
1. Send `headerLeaveRoomRequest`
2. Other players should receive `headerRoomPlayerLeft`
3. If host leaves, host should transfer to another player

### Test 6: Disconnect Handling
1. Disconnect client while in room
2. Other players should receive `headerRoomPlayerLeft`
3. Empty rooms should auto-delete

---

## 📊 Network Flow Example

### Creating and Joining a Room:

```
Client A (Host)                 Server                  Client B (Joiner)
     |                            |                            |
     | -- Login Request --------> |                            |
     | <-- Login Success -------- |                            |
     |                            |                            |
     | -- Create Room Request --> |                            |
     |    (name, mode, etc)       |                            |
     | <-- Create Room Success - |                            |
     |    (roomId: 1)             |                            |
     |                            |                            |
     |                            | <-- Login Request -------- |
     |                            | --> Login Success -------- |
     |                            |                            |
     |                            | <-- Get Room List -------- |
     |                            | --> Room List [Room 1] --- |
     |                            |                            |
     |                            | <-- Join Room Request ---- |
     |                            |    (roomId: 1)             |
     | <-- Player Joined -------- |                            |
     |    (Client B)              | --> Join Success --------- |
     |                            |                            |
     | -- Set Ready (true) -----> |                            |
     | <-- Ready Changed -------- | --> Ready Changed -------- |
     |    (Client A: ready)       |    (Client A: ready)       |
     |                            |                            |
     |                            | <-- Set Ready (true) ----- |
     | <-- Ready Changed -------- | --> Ready Changed -------- |
     |    (Client B: ready)       |    (Client B: ready)       |
     |                            |                            |
     | -- Start Game Request ---> |                            |
     | <-- Status Changed ------- | --> Status Changed ------- |
     |    (IN_GAME)               |    (IN_GAME)               |
     |                            |                            |
```

---

## 🔧 Debugging Tips

### Enable Verbose Logging
The RoomHandler already includes cout statements for all major operations. Watch the server console for:
- Room created messages
- Player join/leave messages
- Ready status changes
- Game start notifications
- Error messages

### Common Issues

**"Not logged in" error:**
- Client must login first before room operations
- Check that session is valid and not expired

**"Already in a room" error:**
- Player can only be in one room at a time
- Leave current room before joining another

**"Room not found" error:**
- Room may have been deleted (empty or inactive)
- Refresh room list and try again

**"Incorrect password" error:**
- Password-protected room requires correct password
- Empty string for public rooms

**"Game already started" error:**
- Can't join rooms that are IN_GAME status
- Wait for game to end or join different room

**Can't start game:**
- Need at least 2 players
- All players must be ready
- Room must be in WAITING status
- Only host can start game

---

## ✅ Integration Checklist

- [ ] RoomManager initialized in server
- [ ] RoomHandler initialized with managers
- [ ] All room packet types added to switch statement
- [ ] Disconnect handler calls roomHandler->handlePlayerDisconnect()
- [ ] Periodic cleanup added to main loop
- [ ] Proper cleanup in server shutdown
- [ ] Tested room creation
- [ ] Tested room joining
- [ ] Tested ready system
- [ ] Tested game starting
- [ ] Tested disconnect handling

---

## 🚀 Ready for Phase 4!

The backend is now **100% complete**. Next step is to create the **Room UI** so players can:
- Browse available rooms
- Create custom rooms
- Join rooms
- See other players in lobby
- Ready up and start games

The UI will use these packet types to communicate with the server through the RoomHandler you just integrated!

---

## 📈 Points Status

**Achieved So Far:**
- ✅ Room Data Structures (1 point)
- ✅ Room Management System (2 points)
- ✅ Network Protocol (1 point)
- ✅ Room Handler Logic (2 points)

**Total: 6/12 points**

**Remaining:**
- 🔄 Room UI (3 points)
- 🔄 Full Integration & Testing (3 points)

**After Phase 4, you'll have earned 8-12 points!**

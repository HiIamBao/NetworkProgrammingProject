# Plan: Multiple Rooms Per Host

## Overview
Enable a single game instance to host multiple game rooms simultaneously, each on a different port.

## Current Architecture Issues

### Problem 1: Fixed Server Port
```cpp
void serverFunction()  // No parameters - hardcoded to port 7778
{
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 7778;  // FIXED PORT
    // ...
}
```

### Problem 2: Global Server State
```cpp
static ENetHost* server = nullptr;  // Only ONE server instance
```

### Problem 3: Single Thread Model
- Only one `serverFunction()` can run at a time
- No way to manage multiple server threads

## Solution Architecture

### Approach 1: Multi-Port Server (Recommended)
**Best for:** Simple implementation, each room is independent

```
┌─────────────────────────────────────┐
│         Host Machine                │
│                                     │
│  ┌──────────────────────────────┐  │
│  │  Room Manager Process        │  │
│  │                              │  │
│  │  Room 1: "Alice's Room"      │  │
│  │  ├─ Port: 7778               │  │
│  │  ├─ Thread 1                 │  │
│  │  └─ Server Instance 1        │  │
│  │                              │  │
│  │  Room 2: "Bob's Room"        │  │
│  │  ├─ Port: 7779               │  │
│  │  ├─ Thread 2                 │  │
│  │  └─ Server Instance 2        │  │
│  │                              │  │
│  │  Room 3: "Charlie's Room"    │  │
│  │  ├─ Port: 7780               │  │
│  │  ├─ Thread 3                 │  │
│  │  └─ Server Instance 3        │  │
│  └──────────────────────────────┘  │
│                                     │
│  LAN Discovery Broadcaster          │
│  ├─ Broadcasts all 3 rooms          │
│  └─ UDP Port: 7775 (different!)    │
└─────────────────────────────────────┘
```

### Approach 2: Single-Port Multiplexer (Advanced)
**Best for:** Maximum port efficiency, complex implementation

```
┌─────────────────────────────────────┐
│         Host Machine                │
│                                     │
│  ┌──────────────────────────────┐  │
│  │  Master Server (Port 7778)   │  │
│  │                              │  │
│  │  Connection Router           │  │
│  │  ├─ Reads room ID from       │  │
│  │  │   initial packet           │  │
│  │  └─ Routes to correct room   │  │
│  │                              │  │
│  │  ┌─────────────────────────┐ │  │
│  │  │ Room 1 (ID: 001)        │ │  │
│  │  │ Room 2 (ID: 002)        │ │  │
│  │  │ Room 3 (ID: 003)        │ │  │
│  │  └─────────────────────────┘ │  │
│  └──────────────────────────────┘  │
└─────────────────────────────────────┘
```

## Implementation Plan: Approach 1 (Multi-Port)

### Phase 1: Refactor Server Code (2-3 hours)

#### Step 1.1: Make Server Port Configurable
**File: `include/gameLayer/serverClient.h`**
```cpp
// OLD:
void serverFunction();

// NEW:
void serverFunction(int port);
```

**File: `src/gameLayer/server.cpp` (or wherever serverFunction is defined)**
```cpp
void serverFunction(int port)
{
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;  // Use parameter instead of hardcoded 7778
    
    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    // ... rest of server code
}
```

#### Step 1.2: Create Multi-Server Manager
**New File: `include/gameLayer/MultiServerManager.h`**
```cpp
#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <string>

struct ServerInstance {
    int port;
    int roomId;
    std::string roomName;
    std::string hostName;
    int maxPlayers;
    int currentPlayers;
    std::thread serverThread;
    std::atomic<bool> running;
};

class MultiServerManager {
public:
    MultiServerManager();
    ~MultiServerManager();
    
    // Create a new server room
    int createRoom(const std::string& roomName, 
                   const std::string& hostName,
                   int maxPlayers);
    
    // Stop a specific room
    void stopRoom(int roomId);
    
    // Get available port (7778-7788 range)
    int getAvailablePort();
    
    // Get all active rooms
    std::vector<ServerInstance*> getActiveRooms();
    
    // Update room player count
    void updateRoomPlayers(int roomId, int playerCount);
    
private:
    std::vector<std::unique_ptr<ServerInstance>> servers;
    std::mutex serversMutex;
    int nextRoomId;
    
    static constexpr int BASE_PORT = 7778;
    static constexpr int MAX_ROOMS = 10;
};
```

#### Step 1.3: Integrate with LAN Discovery
**Modify: `src/gameLayer/LANDiscovery.cpp`**
```cpp
// NEW: Broadcast multiple rooms
void LANDiscovery::broadcastMultipleRooms(
    const std::vector<ServerInstance*>& rooms)
{
    for (const auto& room : rooms) {
        char message[256];
        snprintf(message, sizeof(message), 
                 "GAMESERVER|%s|%s|%d|%d|%d|%d",  // Added roomId
                 room->roomName.c_str(), 
                 room->hostName.c_str(),
                 room->port,
                 room->currentPlayers,
                 room->maxPlayers,
                 room->roomId);
        
        // Send broadcast
        sendto(sockfd, message, strlen(message), 0, 
               (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
    }
}
```

### Phase 2: Update UI Integration (1-2 hours)

#### Step 2.1: Modify Room Creation Callback
**File: `src/gameLayer/gameLayer.cpp`**
```cpp
// OLD:
g_roomUI->onCreateRoom = [&name, &state, &ip](const CreateRoomData& data) {
    std::thread t(serverFunction);  // Fixed port
    t.detach();
    // ...
};

// NEW:
g_roomUI->onCreateRoom = [&name, &state, &ip](const CreateRoomData& data) {
    if (g_multiServerManager) {
        int roomId = g_multiServerManager->createRoom(
            data.roomName,
            g_accountUI->getCurrentUsername(),
            data.maxPlayers
        );
        
        if (roomId > 0) {
            std::cout << "Created room ID: " << roomId << std::endl;
            
            // Update LAN discovery to broadcast all rooms
            auto rooms = g_multiServerManager->getActiveRooms();
            if (g_lanDiscovery) {
                g_lanDiscovery->broadcastMultipleRooms(rooms);
            }
        }
    }
};
```

#### Step 2.2: Add Room Management UI
```cpp
// Show active rooms hosted by this machine
void renderMyRooms() {
    glui::Text("My Hosted Rooms:", Colors_White);
    
    auto myRooms = g_multiServerManager->getActiveRooms();
    for (const auto& room : myRooms) {
        char roomInfo[128];
        snprintf(roomInfo, sizeof(roomInfo), 
                 "Room: %s | Port: %d | Players: %d/%d",
                 room->roomName.c_str(),
                 room->port,
                 room->currentPlayers,
                 room->maxPlayers);
        glui::Text(roomInfo, Colors_White);
        
        if (glui::Button("Close Room", Colors_Error)) {
            g_multiServerManager->stopRoom(room->roomId);
        }
    }
}
```

### Phase 3: Handle Port Conflicts (1 hour)

#### Port Allocation Strategy
```cpp
int MultiServerManager::getAvailablePort() {
    std::lock_guard<std::mutex> lock(serversMutex);
    
    // Try ports 7778-7788
    for (int port = BASE_PORT; port < BASE_PORT + MAX_ROOMS; port++) {
        bool inUse = false;
        
        // Check if port is used by our servers
        for (const auto& server : servers) {
            if (server->port == port && server->running.load()) {
                inUse = true;
                break;
            }
        }
        
        if (!inUse) {
            // Try to bind to verify it's actually available
            if (isPortAvailable(port)) {
                return port;
            }
        }
    }
    
    return -1;  // No available ports
}

bool MultiServerManager::isPortAvailable(int port) {
    SOCKET testSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (testSocket == INVALID_SOCKET) return false;
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    bool available = (bind(testSocket, (sockaddr*)&addr, sizeof(addr)) == 0);
    closesocket(testSocket);
    
    return available;
}
```

### Phase 4: Update Broadcast Protocol (30 mins)

#### New Broadcast Message Format
```
OLD: "GAMESERVER|roomName|hostName|port|players|maxPlayers"
NEW: "GAMESERVER|roomId|roomName|hostName|port|players|maxPlayers"
```

**Example:**
```
"GAMESERVER|1|Alice's Room|Alice|7778|2|4"
"GAMESERVER|2|Bob's Room|Alice|7779|1|4"
"GAMESERVER|3|Test Room|Alice|7780|0|8"
```

### Phase 5: Client Join Logic (30 mins)

```cpp
g_roomUI->onJoinRoom = [&state, &ip](const JoinRoomData& data) {
    if (g_lanDiscovery) {
        auto servers = g_lanDiscovery->getDiscoveredServers();
        
        // Find by roomId instead of index
        for (const auto& server : servers) {
            if (server.roomId == data.roomId) {
                strncpy(ip, server.ipAddress.c_str(), 16);
                int port = server.port;  // Use specific port
                
                // Connect to server at ip:port
                resetClient();
                connectToServer(ip, port);
                state = 1;
                break;
            }
        }
    }
};
```

## Alternative: Simpler Hybrid Approach

If full multi-room is too complex, consider this hybrid:

### Limited Multi-Room (3 Rooms Max)
```cpp
static const int MAX_ROOMS = 3;
static const int ROOM_PORTS[MAX_ROOMS] = {7778, 7779, 7780};

struct SimpleRoomSlot {
    bool active;
    std::thread serverThread;
    std::string roomName;
    int port;
};

static SimpleRoomSlot g_roomSlots[MAX_ROOMS];

int createSimpleRoom(const std::string& roomName) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!g_roomSlots[i].active) {
            g_roomSlots[i].active = true;
            g_roomSlots[i].roomName = roomName;
            g_roomSlots[i].port = ROOM_PORTS[i];
            
            g_roomSlots[i].serverThread = std::thread([i]() {
                serverFunction(ROOM_PORTS[i]);
            });
            g_roomSlots[i].serverThread.detach();
            
            return i;  // Return slot index
        }
    }
    return -1;  // No available slots
}
```

## Pros and Cons

### Approach 1: Multi-Port Server
**Pros:**
- ✅ Clean separation between rooms
- ✅ Easy to implement
- ✅ Each room is independent
- ✅ Crash in one room doesn't affect others
- ✅ Simple debugging

**Cons:**
- ❌ Uses multiple ports (may hit firewall limits)
- ❌ Limited by available ports (typically 10-20 rooms max)
- ❌ More resource intensive (thread per room)

### Approach 2: Single-Port Multiplexer
**Pros:**
- ✅ Only uses one port
- ✅ More efficient resource usage
- ✅ Unlimited rooms (in theory)

**Cons:**
- ❌ Complex to implement
- ❌ Requires protocol changes
- ❌ Single point of failure
- ❌ Harder to debug

### Hybrid: Limited Multi-Room
**Pros:**
- ✅ Simple to implement (2-3 hours)
- ✅ Good for most use cases (3-5 rooms is usually enough)
- ✅ Minimal code changes

**Cons:**
- ❌ Hard limit on rooms
- ❌ Still uses multiple ports

## Recommended Implementation

I recommend **Hybrid Approach** for your project:

1. **Quick Win**: Support 3-5 rooms with pre-defined ports
2. **Simple**: Minimal code changes to existing architecture
3. **Sufficient**: Most players won't host more than 3 rooms anyway
4. **Extendable**: Can upgrade to full multi-port later if needed

## Time Estimates

| Approach | Development Time | Testing | Total |
|----------|-----------------|---------|-------|
| Multi-Port (Full) | 4-5 hours | 2 hours | 6-7 hours |
| Single-Port Multiplexer | 8-10 hours | 4 hours | 12-14 hours |
| Hybrid (Limited) | 2-3 hours | 1 hour | 3-4 hours |

## Implementation Priority

### Phase 1 (Critical):
1. Modify `serverFunction()` to accept port parameter
2. Create array of 3 room slots
3. Test basic multi-room creation

### Phase 2 (Important):
4. Update LAN discovery to broadcast multiple rooms
5. Update room browser to show port info
6. Test joining different rooms

### Phase 3 (Polish):
7. Add UI to manage hosted rooms
8. Add room slot indicators
9. Handle port conflicts gracefully

## Testing Plan

### Test Case 1: Create Multiple Rooms
1. Start game instance
2. Create "Room A" → Should use port 7778
3. Create "Room B" → Should use port 7779
4. Create "Room C" → Should use port 7780
5. Try create "Room D" → Should show "Maximum rooms reached"

### Test Case 2: Different Machines See All Rooms
1. Machine A hosts "Room A" and "Room B"
2. Machine B should see both rooms in browser
3. Join "Room B" → Should connect to correct port

### Test Case 3: Close and Recreate
1. Create 3 rooms
2. Close middle room
3. Create new room → Should reuse freed port
4. All clients should see updated list

## Conclusion

The **Hybrid Limited Multi-Room** approach is the best balance of:
- ✅ Implementation simplicity
- ✅ Sufficient functionality
- ✅ Low risk
- ✅ Quick to implement (3-4 hours)

This gets you 3-5 rooms per host, which is more than enough for most scenarios, while keeping the codebase maintainable.

**Next Steps:**
1. Backup current working code
2. Implement Phase 1 (port parameter)
3. Test single room still works
4. Implement room slots
5. Test multi-room creation
6. Update LAN discovery
7. Test end-to-end

Would you like me to start implementing the Hybrid approach?

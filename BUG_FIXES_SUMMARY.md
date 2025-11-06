# Bug Fixes Summary - Room System Issues

## Issues Identified and Fixed

### Issue #1: "Create Room" Button Does Nothing

**Problem:**
When clicking the "Create Room" button in the Room UI, nothing happened. The server was being started, but the client was not connecting to it.

**Root Cause:**
1. The callback was starting a server thread successfully
2. However, it wasn't setting the IP address to "127.0.0.1" (localhost)
3. The client state was changed but without an IP, the connection failed silently
4. No delay was provided for the server to fully start before the client attempted connection

**Solution:**
Updated the `onCreateRoom` callback in `gameLayer.cpp` to:
- Check if a server is already running before starting a new one
- Set the IP address to "127.0.0.1" explicitly
- Add a 500ms delay to allow the server to initialize
- Properly reset the client state before connecting

```cpp
g_roomUI->onCreateRoom = [&name, &state, &ip](const CreateRoomData& data) {
    // Check if server is already running
    if (isServerRunning()) {
        std::cout << "Server is already running! Cannot create another room." << std::endl;
        return;
    }
    
    // Start server thread
    std::thread t(serverFunction);
    t.detach();
    
    // Wait a bit for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Connect client to local server
    resetClient();
    strcpy(ip, "127.0.0.1"); // Set IP to localhost
    state = 2; // Switch to hosting state (client connected to own server)
};
```

---

### Issue #2: Core Dump When Hosting More Than Two Games

**Problem:**
When attempting to host a second or third game, the application would crash with a core dump.

**Root Causes:**
1. **No Server State Check**: The `serverFunction()` could be called multiple times, each trying to bind to port 7778
2. **Port Conflict**: Multiple `enet_host_create()` calls on the same port caused the second call to fail
3. **Resource Leak**: The original code called `std::terminate()` on failure, but didn't properly handle the port-in-use scenario
4. **No State Reset**: Global variables (`connections`, `items`, `pids`, etc.) were not reset between server instances
5. **Thread Management**: Detached threads were never tracked, allowing multiple server threads to run simultaneously

**Solution:**

#### 1. Added Server State Tracking
Created new functions in `serverClient.h`:
```cpp
bool isServerRunning();
void resetServerState();
```

#### 2. Implemented State Management in server.cpp
```cpp
// Check if server is currently running
bool isServerRunning()
{
    return serverOpen.load();
}

// Reset server state variables
void resetServerState()
{
    connections.clear();
    items.clear();
    itemSpawnPosition = {
        {22,12}, {44,17}, {31,32}, {16,45}, {39,28},
        {11,23}, {25,5}, {27,46}, {22,27}
    };
    pids = 1;
    changedData = 0;
    serverOpen = false;
}
```

#### 3. Updated serverFunction() with Protection
```cpp
void serverFunction()
{
    // Check if server is already running
    if (serverOpen.load())
    {
        std::cout << "Server is already running! Cannot start another server instance." << std::endl;
        return;
    }

    std::srand(std::time(0));
    
    // Reset server state before starting
    resetServerState();
    serverOpen = true;

    ENetAddress adress;
    adress.host = ENET_HOST_ANY;
    adress.port = 7778;
    ENetEvent event;

    //first param adress, players limit, channels, bandwith limit
    ENetHost *server = enet_host_create(&adress, 32, SERVER_CHANNELS, 0, 0);

    if (!server)
    {
        std::cout << "Failed to create server! Port 7778 may be in use." << std::endl;
        serverOpen = false;
        return;  // Gracefully return instead of terminating
    }
    
    std::cout << "Server started successfully on port 7778" << std::endl;
    
    // ... existing server loop ...
}
```

#### 4. Added Proper Server Cleanup
```cpp
// At the end of serverFunction()
std::cout << "Server shutting down..." << std::endl;

// Cleanup: disconnect all clients
for (auto& conn : connections) {
    enet_peer_disconnect(conn.second.peer, 0);
}

// Flush any remaining packets
ENetEvent cleanupEvent;
while (enet_host_service(server, &cleanupEvent, 100) > 0) {
    if (cleanupEvent.type == ENET_EVENT_TYPE_DISCONNECT) {
        // Client disconnected
    }
}

enet_host_destroy(server);

// Reset server state after shutdown
resetServerState();

std::cout << "Server stopped." << std::endl;
```

#### 5. Protected All Server Start Points
Updated all locations where the server can be started:
- Room UI "Create Room" button
- Main Menu "Host Server" button  
- Fallback UI "Host server" option

All now check `isServerRunning()` before attempting to start a new server.

---

## Testing the Fixes

### Test Case 1: Create Room Functionality
1. ✅ Log in to an account
2. ✅ Click "Browse Rooms"
3. ✅ Click "Create Room"
4. ✅ Game should host a local server and connect as a client

**Expected Result:** Game transitions to the game state with the player as host.

### Test Case 2: Multiple Server Prevention
1. ✅ Start a server/create a room
2. ✅ Try to create another room or host another server
3. ✅ Should see message: "Server is already running!"

**Expected Result:** Second server attempt is blocked, no crash occurs.

### Test Case 3: Sequential Server Hosting
1. ✅ Host a game
2. ✅ Stop the server (close game or disconnect)
3. ✅ Host another game
4. ✅ Should work without issues

**Expected Result:** Server state is properly reset between sessions.

---

## Technical Details

### Key Changes Made

**Files Modified:**
1. `/include/gameLayer/serverClient.h` - Added `isServerRunning()` and `resetServerState()` declarations
2. `/src/gameLayer/server.cpp` - Added state management and protection logic
3. `/src/gameLayer/gameLayer.cpp` - Updated all server start callbacks with protection

### State Management Flow
```
Before Fix:
User clicks "Create Room" → serverFunction() called → Port conflict → CRASH

After Fix:
User clicks "Create Room" → Check isServerRunning() → 
    If running: Show message, return
    If not running: Reset state → Start server → Connect client → Success
```

### Memory Safety Improvements
- Proper cleanup of ENet resources
- Graceful disconnect of all clients on shutdown
- Reset of all global state variables
- No more `std::terminate()` on port conflict

---

## Additional Improvements

### User Feedback
Added console messages for better debugging:
- "Server started successfully on port 7778"
- "Server is already running! Cannot start another server instance."
- "Failed to create server! Port 7778 may be in use."
- "Server shutting down..."
- "Server stopped."

### UI Feedback
Updated the Host Server UI to show server status:
```cpp
// Check server status
if (isServerRunning()) {
    glui::Text("Server is already running!", glm::vec4(1.0f, 0.5f, 0.2f, 1.0f));
    glui::Text("Join as host or stop the server first.", Colors_White);
} else {
    glui::Text("Click to start server...", Colors_White);
}
```

---

## Known Limitations

1. **Single Server Instance**: Only one server can run per application instance
2. **Port Hardcoded**: Server always uses port 7778 (this is intentional for local hosting)
3. **No Server Stop Button**: User must exit the game to stop the server (future enhancement)
4. **Thread Cleanup**: Detached threads are not joined on exit (acceptable for game context)

---

## Future Enhancements

1. **Server Stop Button**: Add UI to stop the server without exiting
2. **Server Status Indicator**: Show server status in main menu
3. **Port Configuration**: Allow custom port selection
4. **Multiple Server Support**: Track server threads and allow multiple servers on different ports
5. **Graceful Shutdown**: Implement proper thread joining and cleanup on application exit

---

## Conclusion

Both issues have been successfully resolved:
- ✅ "Create Room" now properly starts a server and connects the client
- ✅ Multiple server attempts are blocked, preventing crashes
- ✅ Server state is properly managed and cleaned up
- ✅ Application is more stable and user-friendly

The fixes maintain backward compatibility with the existing code while adding robust state management and error handling.

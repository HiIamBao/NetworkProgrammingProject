# Account & Session Management System - Integration Guide

## Overview
This implementation adds a complete account registration, login, and session management system to your multiplayer game.

## Features Implemented

### ✅ Account System (2 points - Đăng ký và quản lý tài khoản)
- User registration with username, password, and email
- Password hashing using SHA-256
- SQLite database for persistent storage
- Account statistics tracking (level, score, games played/won)
- Input validation and duplicate checking

### ✅ Session Management (2 points - Đăng nhập và quản lý phiên)
- Secure session token generation
- Session validation and timeout (1 hour)
- Multi-device login prevention
- Activity tracking
- Automatic session cleanup

### ✅ Stream Processing (1 point - Xử lý truyền dòng)
- Packet-based authentication protocol
- Reliable/unreliable packet handling via ENet

### ✅ Socket I/O Management (2 points - Cơ chế vào/ra socket)
- ENetPeer-based session tracking
- Proper disconnect handling
- Multiple packet types for auth operations

## Files Created

### Header Files:
1. `include/gameLayer/AccountManager.h` - Account management interface
2. `include/gameLayer/SessionManager.h` - Session management interface
3. `include/gameLayer/AuthenticationHandler.h` - Server-side auth handler

### Implementation Files:
1. `src/gameLayer/AccountManager.cpp` - Account database operations
2. `src/gameLayer/SessionManager.cpp` - Session lifecycle management
3. `src/gameLayer/AuthenticationHandler.cpp` - Packet handling for auth

### Modified Files:
1. `include/gameLayer/packet.h` - Added auth packet types
2. `CMakeLists.txt` - Added SQLite3 and OpenSSL dependencies

## Server Integration Example

### 1. Initialize in Server

```cpp
// In server.cpp or your server initialization
#include "AccountManager.h"
#include "SessionManager.h"
#include "AuthenticationHandler.h"

// Global instances
AccountManager* g_accountManager = nullptr;
SessionManager* g_sessionManager = nullptr;
AuthenticationHandler* g_authHandler = nullptr;

void serverFunction() {
    // Initialize ENet
    if (enet_initialize() != 0) {
        return;
    }
    
    // Initialize account system
    g_accountManager = new AccountManager();
    if (!g_accountManager->initialize("./game_accounts.db")) {
        std::cerr << "Failed to initialize account manager!" << std::endl;
        return;
    }
    
    // Initialize session system
    g_sessionManager = new SessionManager(g_accountManager);
    
    // Initialize authentication handler
    g_authHandler = new AuthenticationHandler(g_accountManager, g_sessionManager);
    
    // Create ENet server
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 7777;
    
    ENetHost* server = enet_host_create(&address, 32, SERVER_CHANNELS, 0, 0);
    if (!server) {
        std::cerr << "Failed to create server!" << std::endl;
        return;
    }
    
    std::cout << "Server started on port 7777" << std::endl;
    
    // Main server loop
    while (true) {
        ENetEvent event;
        while (enet_host_service(server, &event, 100) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "New client connected" << std::endl;
                    break;
                    
                case ENET_EVENT_TYPE_DISCONNECT:
                    std::cout << "Client disconnected" << std::endl;
                    g_authHandler->handlePeerDisconnect(event.peer);
                    break;
                    
                case ENET_EVENT_TYPE_RECEIVE:
                    handlePacket(event);
                    enet_packet_destroy(event.packet);
                    break;
            }
        }
        
        // Clean expired sessions every 60 seconds
        static auto lastCleanup = std::chrono::system_clock::now();
        auto now = std::chrono::system_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCleanup).count() > 60) {
            g_sessionManager->cleanExpiredSessions();
            lastCleanup = now;
        }
    }
    
    // Cleanup
    enet_host_destroy(server);
    delete g_authHandler;
    delete g_sessionManager;
    delete g_accountManager;
    enet_deinitialize();
}
```

### 2. Handle Authentication Packets

```cpp
void handlePacket(ENetEvent& event) {
    Packet packet;
    size_t dataSize;
    char* data = parsePacket(event, packet, dataSize);
    
    switch (packet.header) {
        case headerRegisterRequest:
            g_authHandler->handleRegisterRequest(event.peer, data, dataSize);
            break;
            
        case headerLoginRequest:
            g_authHandler->handleLoginRequest(event.peer, data, dataSize);
            break;
            
        case headerLogoutRequest:
            g_authHandler->handleLogoutRequest(event.peer, data, dataSize);
            break;
            
        case headerRequestAccountInfo:
            g_authHandler->handleAccountInfoRequest(event.peer, data, dataSize);
            break;
            
        case headerRequestLeaderboard:
            g_authHandler->handleLeaderboardRequest(event.peer);
            break;
            
        // Game packets - validate session first
        case headerUpdateConnection:
        case headerSendBullet:
        case headerPickupItem: {
            std::string username;
            if (g_authHandler->validateSessionForAction(event.peer, username)) {
                // Process game action
                handleGameAction(event.peer, packet, data, dataSize, username);
            } else {
                std::cerr << "Unauthorized game action attempt" << std::endl;
            }
            break;
        }
        
        default:
            std::cerr << "Unknown packet type: " << packet.header << std::endl;
            break;
    }
}
```

### 3. Client-Side Integration Example

```cpp
// In client.cpp
#include "AuthenticationHandler.h"

class GameClient {
private:
    ENetHost* client;
    ENetPeer* serverPeer;
    std::string sessionToken;
    bool isLoggedIn;
    
public:
    void sendRegisterRequest(const std::string& username, const std::string& password, const std::string& email) {
        RegisterData regData;
        strncpy(regData.username, username.c_str(), sizeof(regData.username) - 1);
        strncpy(regData.password, password.c_str(), sizeof(regData.password) - 1);
        strncpy(regData.email, email.c_str(), sizeof(regData.email) - 1);
        
        Packet packet;
        packet.header = headerRegisterRequest;
        
        sendPacket(serverPeer, packet, reinterpret_cast<char*>(&regData), sizeof(regData), true, 0);
    }
    
    void sendLoginRequest(const std::string& username, const std::string& password) {
        LoginData loginData;
        strncpy(loginData.username, username.c_str(), sizeof(loginData.username) - 1);
        strncpy(loginData.password, password.c_str(), sizeof(loginData.password) - 1);
        
        Packet packet;
        packet.header = headerLoginRequest;
        
        sendPacket(serverPeer, packet, reinterpret_cast<char*>(&loginData), sizeof(loginData), true, 0);
    }
    
    void handleLoginResponse(const char* data, size_t dataSize) {
        LoginResponseData response;
        memcpy(&response, data, sizeof(response));
        
        if (response.success) {
            sessionToken = response.token;
            isLoggedIn = true;
            
            std::cout << "Login successful!" << std::endl;
            std::cout << "Level: " << response.level << std::endl;
            std::cout << "Score: " << response.totalScore << std::endl;
            std::cout << "Win Rate: " << (response.winRate * 100) << "%" << std::endl;
        } else {
            std::cout << "Login failed: " << response.message << std::endl;
        }
    }
    
    void requestLeaderboard() {
        if (!isLoggedIn) return;
        
        Packet packet;
        packet.header = headerRequestLeaderboard;
        sendPacket(serverPeer, packet, nullptr, 0, true, 0);
    }
};
```

## Database Schema

The system creates the following SQLite table:

```sql
CREATE TABLE accounts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL COLLATE NOCASE,
    password_hash TEXT NOT NULL,
    email TEXT UNIQUE NOT NULL COLLATE NOCASE,
    level INTEGER DEFAULT 1,
    total_score INTEGER DEFAULT 0,
    games_played INTEGER DEFAULT 0,
    games_won INTEGER DEFAULT 0,
    win_rate REAL DEFAULT 0.0,
    ranking INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

## Building

Before building, install the required dependencies:

```bash
sudo apt install -y libsqlite3-dev libssl-dev
```

Then rebuild:

```bash
cd build
cmake ..
make
```

## Testing the System

### 1. Start Server
```bash
cd build
./multyPlayer server
```

### 2. Connect Client & Register
```bash
# Client sends registration
Username: testuser
Password: test123456
Email: test@example.com
```

### 3. Login
```bash
# Client sends login
Username: testuser
Password: test123456
# Receives session token
```

### 4. Play Game
All game actions now require valid session

### 5. View Stats
Request account info or leaderboard

## Security Features

1. **Password Hashing**: SHA-256 (consider bcrypt for production)
2. **Session Tokens**: 256-bit random tokens
3. **Session Timeout**: 1 hour of inactivity
4. **Input Validation**: Username, password, email checks
5. **Duplicate Prevention**: Username and email uniqueness
6. **Thread Safety**: Mutex-protected operations

## Points Achieved

- ✅ Xử lý truyền dòng (Stream Processing): **1 điểm**
- ✅ Cài đặt cơ chế vào/ra socket (Socket I/O): **2 điểm**
- ✅ Đăng ký và quản lý tài khoản (Account Registration): **2 điểm**
- ✅ Đăng nhập và quản lý phiên (Login & Session): **2 điểm**

**Total: 7/35 points** from requirements list

## Next Steps

To complete the remaining requirements, implement:
1. Room system (1-2 points)
2. Enhanced gameplay management (5-8 points)
3. Game modes (2-8 points)
4. Pause system (2 points)
5. Score/ranking system (3-5 points)
6. Leaderboard (already partially done)
7. Graphics UI improvements (3 points)

## Troubleshooting

### Build Errors
- Ensure SQLite3 and OpenSSL are installed
- Check CMakeLists.txt has correct library paths
- Run `cmake ..` after installing dependencies

### Runtime Errors
- Check database file permissions
- Ensure port 7777 is not in use
- Verify ENet initialization succeeds

### Session Issues
- Check system time is synchronized
- Verify session timeout setting
- Check for proper logout on disconnect

## API Reference

See header files for detailed API documentation:
- `AccountManager.h` - Account CRUD operations
- `SessionManager.h` - Session lifecycle
- `AuthenticationHandler.h` - Packet handling

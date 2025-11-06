#pragma once

#include "RoomManager.h"
#include "SessionManager.h"
#include "AccountManager.h"
#include <enet/enet.h>
#include <string>

class RoomHandler {
private:
    AccountManager* accountManager;
    SessionManager* sessionManager;
    RoomManager* roomManager;
    
    // Helper methods
    bool validateSession(ENetPeer* peer, std::string& username);
    void sendResponse(ENetPeer* peer, int packetHeader, const void* data, size_t size, bool reliable = true);
    void broadcastToRoom(int roomId, int packetHeader, const void* data, size_t size, ENetPeer* excludePeer = nullptr);
    
public:
    RoomHandler(AccountManager* accMgr, SessionManager* sessMgr, RoomManager* roomMgr);
    ~RoomHandler();
    
    // Packet handlers
    void handleCreateRoom(ENetPeer* peer, const char* data, size_t dataSize);
    void handleJoinRoom(ENetPeer* peer, const char* data, size_t dataSize);
    void handleLeaveRoom(ENetPeer* peer);
    void handleGetRoomList(ENetPeer* peer);
    void handleGetRoomInfo(ENetPeer* peer, const char* data, size_t dataSize);
    void handleStartGame(ENetPeer* peer);
    void handleSetReady(ENetPeer* peer, const char* data, size_t dataSize);
    
    // Event handlers
    void handlePlayerDisconnect(ENetPeer* peer);
    
    // Utility
    void cleanupInactiveRooms();
};

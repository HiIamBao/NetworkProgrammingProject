#include "RoomHandler.h"
#include "packet.h"
#include <iostream>
#include <cstring>

RoomHandler::RoomHandler(AccountManager* accMgr, SessionManager* sessMgr, RoomManager* roomMgr)
    : accountManager(accMgr), sessionManager(sessMgr), roomManager(roomMgr) {
    std::cout << "RoomHandler initialized" << std::endl;
}

RoomHandler::~RoomHandler() {
}

bool RoomHandler::validateSession(ENetPeer* peer, std::string& username) {
    if (!sessionManager) {
        return false;
    }
    
    std::string token = sessionManager->getTokenByPeer(peer);
    if (token.empty()) {
        return false;
    }
    
    if (!sessionManager->validateSession(token)) {
        return false;
    }
    
    username = sessionManager->getUsername(token);
    return !username.empty();
}

void RoomHandler::sendResponse(ENetPeer* peer, int packetHeader, const void* data, size_t size, bool reliable) {
    Packet packet;
    packet.header = packetHeader;
    sendPacket(peer, packet, static_cast<const char*>(data), size, reliable, 0);
}

void RoomHandler::broadcastToRoom(int roomId, int packetHeader, const void* data, size_t size, ENetPeer* excludePeer) {
    auto room = roomManager->getRoom(roomId);
    if (!room) {
        return;
    }
    
    const auto& players = room->getPlayers();
    for (const auto& player : players) {
        if (player.peer != excludePeer) {
            sendResponse(player.peer, packetHeader, data, size);
        }
    }
}

void RoomHandler::handleCreateRoom(ENetPeer* peer, const char* data, size_t dataSize) {
    std::string username;
    CreateRoomResponse response;
    response.success = false;
    strcpy(response.message, "Unknown error");
    
    // Validate session
    if (!validateSession(peer, username)) {
        strcpy(response.message, "Not logged in");
        sendResponse(peer, headerCreateRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Check if player is already in a room
    if (roomManager->isPlayerInAnyRoom(username)) {
        strcpy(response.message, "Already in a room");
        sendResponse(peer, headerCreateRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Parse request data
    if (dataSize < sizeof(CreateRoomData)) {
        strcpy(response.message, "Invalid request data");
        sendResponse(peer, headerCreateRoomResponse, &response, sizeof(response));
        return;
    }
    
    CreateRoomData roomData;
    memcpy(&roomData, data, sizeof(CreateRoomData));
    
    // Validate room data
    if (strlen(roomData.roomName) == 0) {
        strcpy(response.message, "Room name cannot be empty");
        sendResponse(peer, headerCreateRoomResponse, &response, sizeof(response));
        return;
    }
    
    if (roomData.maxPlayers < 2 || roomData.maxPlayers > 8) {
        strcpy(response.message, "Max players must be 2-8");
        sendResponse(peer, headerCreateRoomResponse, &response, sizeof(response));
        return;
    }
    
    if (roomData.gameMode < 0 || roomData.gameMode > 2) {
        strcpy(response.message, "Invalid game mode");
        sendResponse(peer, headerCreateRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Create room
    int roomId = roomManager->createRoom(
        roomData.roomName,
        username,
        roomData.password,
        roomData.maxPlayers,
        static_cast<GameMode>(roomData.gameMode),
        roomData.mapId
    );
    
    if (roomId < 0) {
        strcpy(response.message, "Failed to create room");
        sendResponse(peer, headerCreateRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Add creator as first player
    if (!roomManager->addPlayerToRoom(roomId, username, peer)) {
        roomManager->deleteRoom(roomId);
        strcpy(response.message, "Failed to join created room");
        sendResponse(peer, headerCreateRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Success
    response.success = true;
    response.roomId = roomId;
    strcpy(response.message, "Room created successfully");
    sendResponse(peer, headerCreateRoomResponse, &response, sizeof(response));
    
    std::cout << "Room created: ID=" << roomId << ", Name=" << roomData.roomName 
              << ", Host=" << username << std::endl;
}

void RoomHandler::handleJoinRoom(ENetPeer* peer, const char* data, size_t dataSize) {
    std::string username;
    JoinRoomResponse response;
    response.success = false;
    strcpy(response.message, "Unknown error");
    
    // Validate session
    if (!validateSession(peer, username)) {
        strcpy(response.message, "Not logged in");
        sendResponse(peer, headerJoinRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Check if player is already in a room
    if (roomManager->isPlayerInAnyRoom(username)) {
        strcpy(response.message, "Already in a room");
        sendResponse(peer, headerJoinRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Parse request data
    if (dataSize < sizeof(JoinRoomData)) {
        strcpy(response.message, "Invalid request data");
        sendResponse(peer, headerJoinRoomResponse, &response, sizeof(response));
        return;
    }
    
    JoinRoomData joinData;
    memcpy(&joinData, data, sizeof(JoinRoomData));
    
    // Get room
    auto room = roomManager->getRoom(joinData.roomId);
    if (!room) {
        strcpy(response.message, "Room not found");
        sendResponse(peer, headerJoinRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Check if room is full
    if (room->isFull()) {
        strcpy(response.message, "Room is full");
        sendResponse(peer, headerJoinRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Check if room is in game
    if (room->getStatus() != RoomStatus::WAITING) {
        strcpy(response.message, "Game already started");
        sendResponse(peer, headerJoinRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Validate password
    if (!room->validatePassword(joinData.password)) {
        strcpy(response.message, "Incorrect password");
        sendResponse(peer, headerJoinRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Add player to room
    if (!room->addPlayer(username, peer)) {
        strcpy(response.message, "Failed to join room");
        sendResponse(peer, headerJoinRoomResponse, &response, sizeof(response));
        return;
    }
    
    // Success
    response.success = true;
    response.roomId = joinData.roomId;
    strcpy(response.message, "Joined room successfully");
    sendResponse(peer, headerJoinRoomResponse, &response, sizeof(response));
    
    // Broadcast to other players in room
    PlayerJoinedData broadcastData;
    strncpy(broadcastData.username, username.c_str(), sizeof(broadcastData.username) - 1);
    broadcastData.username[sizeof(broadcastData.username) - 1] = '\0';
    broadcastToRoom(joinData.roomId, headerRoomPlayerJoined, &broadcastData, sizeof(broadcastData), peer);
    
    std::cout << "Player " << username << " joined room " << joinData.roomId << std::endl;
}

void RoomHandler::handleLeaveRoom(ENetPeer* peer) {
    std::string username;
    
    // Validate session
    if (!validateSession(peer, username)) {
        return;
    }
    
    // Find room player is in
    auto room = roomManager->findRoomWithPlayer(username);
    if (!room) {
        return;  // Not in any room
    }
    
    int roomId = room->getRoomId();
    bool wasHost = room->isHost(username);
    
    // Remove player
    room->removePlayer(username);
    
    // Broadcast to other players
    PlayerLeftData broadcastData;
    strncpy(broadcastData.username, username.c_str(), sizeof(broadcastData.username) - 1);
    broadcastData.username[sizeof(broadcastData.username) - 1] = '\0';
    broadcastToRoom(roomId, headerRoomPlayerLeft, &broadcastData, sizeof(broadcastData));
    
    // If room is empty, delete it
    if (room->isEmpty()) {
        roomManager->deleteRoom(roomId);
        std::cout << "Room " << roomId << " deleted (empty)" << std::endl;
    } else if (wasHost) {
        // Host changed, notify room
        std::cout << "Host changed in room " << roomId << " to " << room->getHostUsername() << std::endl;
    }
    
    std::cout << "Player " << username << " left room " << roomId << std::endl;
}

void RoomHandler::handleGetRoomList(ENetPeer* peer) {
    std::string username;
    
    // Validate session
    if (!validateSession(peer, username)) {
        return;
    }
    
    // Get available rooms
    auto roomInfoList = roomManager->getAvailableRoomInfo();
    
    // Send response
    RoomListResponse response;
    response.roomCount = roomInfoList.size();
    
    // Send header first
    sendResponse(peer, headerGetRoomListResponse, &response, sizeof(response));
    
    // Send each room info
    for (const auto& roomInfo : roomInfoList) {
        sendResponse(peer, headerGetRoomListResponse, &roomInfo, sizeof(roomInfo));
    }
    
    std::cout << "Sent room list to " << username << " (" << roomInfoList.size() << " rooms)" << std::endl;
}

void RoomHandler::handleGetRoomInfo(ENetPeer* peer, const char* data, size_t dataSize) {
    std::string username;
    
    // Validate session
    if (!validateSession(peer, username)) {
        return;
    }
    
    // Parse request
    if (dataSize < sizeof(int)) {
        return;
    }
    
    int roomId;
    memcpy(&roomId, data, sizeof(int));
    
    // Get room
    auto room = roomManager->getRoom(roomId);
    if (!room) {
        return;
    }
    
    // Prepare detailed room info
    DetailedRoomInfo detailedInfo;
    auto roomInfo = room->getRoomInfo();
    // Copy room info fields
    memcpy(&detailedInfo.info, &roomInfo, sizeof(RoomInfoData));
    detailedInfo.playerCount = room->getCurrentPlayerCount();
    
    // Send header
    sendResponse(peer, headerGetRoomInfoResponse, &detailedInfo, sizeof(detailedInfo));
    
    // Send player list
    const auto& players = room->getPlayers();
    for (const auto& player : players) {
        PlayerInRoomData playerData;
        strncpy(playerData.username, player.username.c_str(), sizeof(playerData.username) - 1);
        playerData.username[sizeof(playerData.username) - 1] = '\0';
        playerData.isReady = player.isReady;
        playerData.team = player.team;
        
        sendResponse(peer, headerGetRoomInfoResponse, &playerData, sizeof(playerData));
    }
    
    std::cout << "Sent detailed room info for room " << roomId << " to " << username << std::endl;
}

void RoomHandler::handleStartGame(ENetPeer* peer) {
    std::string username;
    
    // Validate session
    if (!validateSession(peer, username)) {
        return;
    }
    
    // Find room
    auto room = roomManager->findRoomWithPlayer(username);
    if (!room) {
        return;
    }
    
    // Check if player is host
    if (!room->isHost(username)) {
        std::cout << "Non-host " << username << " tried to start game" << std::endl;
        return;
    }
    
    // Check if game can start
    if (!room->canStart()) {
        std::cout << "Room " << room->getRoomId() << " cannot start (not all ready or < 2 players)" << std::endl;
        return;
    }
    
    // Change room status
    room->setStatus(RoomStatus::IN_GAME);
    
    // Broadcast status change
    RoomStatusChangedData statusData;
    statusData.newStatus = static_cast<int>(RoomStatus::IN_GAME);
    broadcastToRoom(room->getRoomId(), headerRoomStatusChanged, &statusData, sizeof(statusData));
    
    std::cout << "Game started in room " << room->getRoomId() << " by " << username << std::endl;
}

void RoomHandler::handleSetReady(ENetPeer* peer, const char* data, size_t dataSize) {
    std::string username;
    
    // Validate session
    if (!validateSession(peer, username)) {
        return;
    }
    
    // Parse request
    if (dataSize < sizeof(SetReadyData)) {
        return;
    }
    
    SetReadyData readyData;
    memcpy(&readyData, data, sizeof(SetReadyData));
    
    // Find room
    auto room = roomManager->findRoomWithPlayer(username);
    if (!room) {
        return;
    }
    
    // Set ready status
    if (!room->setPlayerReady(username, readyData.ready)) {
        return;
    }
    
    // Broadcast to room
    PlayerReadyChangedData broadcastData;
    strncpy(broadcastData.username, username.c_str(), sizeof(broadcastData.username) - 1);
    broadcastData.username[sizeof(broadcastData.username) - 1] = '\0';
    broadcastData.isReady = readyData.ready;
    broadcastToRoom(room->getRoomId(), headerRoomPlayerReadyChanged, &broadcastData, sizeof(broadcastData));
    
    std::cout << "Player " << username << " set ready=" << readyData.ready 
              << " in room " << room->getRoomId() << std::endl;
}

void RoomHandler::handlePlayerDisconnect(ENetPeer* peer) {
    // Remove player from any room they're in
    auto room = roomManager->findRoomByPeer(peer);
    if (room) {
        std::string username;
        auto player = room->getPlayerByPeer(peer);
        if (player) {
            username = player->username;
            int roomId = room->getRoomId();
            
            // Remove player
            room->removePlayerByPeer(peer);
            
            // Broadcast to other players
            PlayerLeftData broadcastData;
            strncpy(broadcastData.username, username.c_str(), sizeof(broadcastData.username) - 1);
            broadcastData.username[sizeof(broadcastData.username) - 1] = '\0';
            broadcastToRoom(roomId, headerRoomPlayerLeft, &broadcastData, sizeof(broadcastData));
            
            // Delete room if empty
            if (room->isEmpty()) {
                roomManager->deleteRoom(roomId);
                std::cout << "Room " << roomId << " deleted (empty after disconnect)" << std::endl;
            }
            
            std::cout << "Player " << username << " disconnected from room " << roomId << std::endl;
        }
    }
}

void RoomHandler::cleanupInactiveRooms() {
    roomManager->deleteInactiveRooms(1800);  // 30 minutes
    roomManager->deleteEmptyRooms();
}

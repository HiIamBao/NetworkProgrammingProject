#pragma once

#include "GameRoom.h"
#include <map>
#include <vector>
#include <mutex>
#include <memory>

class RoomManager {
private:
    std::map<int, std::shared_ptr<GameRoom>> rooms;
    int nextRoomId;
    mutable std::mutex roomsMutex;
    
public:
    RoomManager();
    ~RoomManager();
    
    // Room creation and deletion
    int createRoom(const std::string& roomName, const std::string& hostUsername,
                   const std::string& password, int maxPlayers, GameMode gameMode, int mapId);
    bool deleteRoom(int roomId);
    void deleteEmptyRooms();
    void deleteInactiveRooms(int timeoutSeconds = 1800); // 30 minutes default
    
    // Room retrieval
    std::shared_ptr<GameRoom> getRoom(int roomId);
    std::vector<std::shared_ptr<GameRoom>> getAllRooms();
    std::vector<std::shared_ptr<GameRoom>> getAvailableRooms();  // Not full, not in-game
    std::vector<GameRoom::RoomInfo> getAllRoomInfo();
    std::vector<GameRoom::RoomInfo> getAvailableRoomInfo();
    
    // Find rooms
    std::shared_ptr<GameRoom> findRoomByHost(const std::string& hostUsername);
    std::shared_ptr<GameRoom> findRoomWithPlayer(const std::string& username);
    std::shared_ptr<GameRoom> findRoomByPeer(ENetPeer* peer);
    
    // Player management
    bool addPlayerToRoom(int roomId, const std::string& username, ENetPeer* peer, const std::string& password = "");
    bool removePlayerFromRoom(int roomId, const std::string& username);
    bool removePlayerFromAllRooms(const std::string& username);
    bool removePlayerByPeer(ENetPeer* peer);
    
    // Room info
    int getRoomCount() const;
    int getActiveRoomCount() const;  // Rooms that are not empty
    int getTotalPlayers() const;
    
    // Validation
    bool isRoomNameTaken(const std::string& roomName) const;
    bool isPlayerInAnyRoom(const std::string& username) const;
    
    // Cleanup
    void clearAllRooms();
};

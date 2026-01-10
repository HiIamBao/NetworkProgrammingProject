#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <mutex>
#include <memory>
#include <fstream>

// Simplified room info without thread (for returning to caller)
struct RoomInfo {
    int slotId;
    int port;
    bool active;
    std::string roomName;
    std::string hostName;
    int maxPlayers;
    int currentPlayers;
    int gameMode;
    int mapId;
    int bossLevel;  // Boss difficulty level (1-3), only used for BOSS_FIGHT mode
};

struct RoomSlot {
    int slotId;
    int port;
    std::atomic<bool> active;
    std::string roomName;
    std::string hostName;
    int maxPlayers;
    std::atomic<int> currentPlayers;
    int gameMode;
    int mapId;
    int bossLevel;  // Boss difficulty level (1-3), only used for BOSS_FIGHT mode
    std::unique_ptr<std::thread> serverThread;
    
    RoomSlot() : active(false), currentPlayers(0), slotId(0), port(0), maxPlayers(4), gameMode(0), mapId(0), bossLevel(1) {}
};

class MultiRoomManager {
public:
    MultiRoomManager();
    ~MultiRoomManager();
    
    // Create a new room, returns slot ID or -1 if failed
    int createRoom(const std::string& roomName, 
                   const std::string& hostName,
                   int maxPlayers,
                   int gameMode = 0,
                   int mapId = 0,
                   int bossLevel = 1);
    
    // Stop a specific room by slot ID
    void stopRoom(int slotId);
    
    // Get all active rooms (returns simplified info)
    std::vector<RoomInfo> getActiveRooms();
    
    // Update room player count
    void updateRoomPlayers(int slotId, int playerCount);
    
    // Get room info by slot ID
    RoomInfo getRoomInfo(int slotId);
    
    // Check if any rooms are active
    bool hasActiveRooms() const;
    
    // Get number of active rooms
    int getActiveRoomCount() const;
    
private:
    static constexpr int MAX_ROOMS = 3;
    static constexpr int BASE_PORT = 7780;
    
    std::unique_ptr<RoomSlot[]> rooms;
    std::mutex roomsMutex;
    
    void initializeRooms();
    
    // Port lock file management
    std::string getPortLockFilePath(int port) const;
    bool isPortLockedByFile(int port) const;
    bool lockPort(int port, const std::string& roomName);
    void unlockPort(int port);
    void cleanupStaleLocks();
};

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <enet/enet.h>

enum class RoomStatus {
    WAITING,    // Waiting for players
    IN_GAME,    // Game in progress
    FINISHED    // Game ended
};

enum class GameMode {
    DEATHMATCH = 0,     // Free for all
    HORDE_DEFENSE = 1,  // Horde Defense - Players fight waves of AI enemies directly
    BOSS_FIGHT = 2,     // Boss Fight - Cooperative boss battle
    TEAM_BATTLE = 3     // Team Battle placeholder
};

struct PlayerInRoom {
    std::string username;
    ENetPeer* peer;
    bool isReady;
    int team;  // 0 or 1 for team modes, -1 for non-team modes
    
    PlayerInRoom(const std::string& name, ENetPeer* p) 
        : username(name), peer(p), isReady(false), team(-1) {}
};

class GameRoom {
private:
    int roomId;
    std::string roomName;
    std::string hostUsername;
    std::string password;
    int maxPlayers;
    GameMode gameMode;
    int mapId;
    RoomStatus status;
    std::vector<PlayerInRoom> players;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastActivity;
    
public:
    GameRoom(int id, const std::string& name, const std::string& host, 
             const std::string& pass, int maxP, GameMode mode, int map);
    
    // Getters
    int getRoomId() const { return roomId; }
    std::string getRoomName() const { return roomName; }
    std::string getHostUsername() const { return hostUsername; }
    bool hasPassword() const { return !password.empty(); }
    int getMaxPlayers() const { return maxPlayers; }
    int getCurrentPlayerCount() const { return players.size(); }
    GameMode getGameMode() const { return gameMode; }
    int getMapId() const { return mapId; }
    RoomStatus getStatus() const { return status; }
    const std::vector<PlayerInRoom>& getPlayers() const { return players; }
    
    // Room operations
    bool addPlayer(const std::string& username, ENetPeer* peer);
    bool removePlayer(const std::string& username);
    bool removePlayerByPeer(ENetPeer* peer);
    PlayerInRoom* getPlayer(const std::string& username);
    PlayerInRoom* getPlayerByPeer(ENetPeer* peer);
    
    // Host operations
    bool isHost(const std::string& username) const;
    bool transferHost(const std::string& newHost);
    
    // Ready system
    bool setPlayerReady(const std::string& username, bool ready);
    bool areAllPlayersReady() const;
    int getReadyPlayerCount() const;
    
    // Team assignment (for team modes)
    bool assignPlayerToTeam(const std::string& username, int team);
    int getTeamPlayerCount(int team) const;
    
    // Room state
    bool isFull() const { return players.size() >= static_cast<size_t>(maxPlayers); }
    bool isEmpty() const { return players.empty(); }
    bool canStart() const;
    
    // Password validation
    bool validatePassword(const std::string& pass) const;
    
    // Status management
    void setStatus(RoomStatus newStatus) { status = newStatus; }
    void updateActivity();
    bool isInactive(int timeoutSeconds) const;
    
    // Room info for network transmission
    struct RoomInfo {
        int roomId;
        char roomName[32];
        char hostUsername[32];
        int currentPlayers;
        int maxPlayers;
        int gameMode;
        int mapId;
        bool hasPassword;
        int status;  // RoomStatus as int
    };
    
    RoomInfo getRoomInfo() const;
};

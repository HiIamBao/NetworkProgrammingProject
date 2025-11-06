#include "GameRoom.h"
#include <algorithm>
#include <cstring>

GameRoom::GameRoom(int id, const std::string& name, const std::string& host,
                   const std::string& pass, int maxP, GameMode mode, int map)
    : roomId(id), roomName(name), hostUsername(host), password(pass),
      maxPlayers(maxP), gameMode(mode), mapId(map), status(RoomStatus::WAITING) {
    createdAt = std::chrono::system_clock::now();
    lastActivity = createdAt;
}

bool GameRoom::addPlayer(const std::string& username, ENetPeer* peer) {
    if (isFull()) {
        return false;
    }
    
    // Check if player already in room
    for (const auto& p : players) {
        if (p.username == username) {
            return false;
        }
    }
    
    players.emplace_back(username, peer);
    updateActivity();
    return true;
}

bool GameRoom::removePlayer(const std::string& username) {
    auto it = std::find_if(players.begin(), players.end(),
        [&username](const PlayerInRoom& p) { return p.username == username; });
    
    if (it != players.end()) {
        players.erase(it);
        updateActivity();
        
        // If host left and there are still players, transfer host
        if (username == hostUsername && !players.empty()) {
            hostUsername = players[0].username;
        }
        
        return true;
    }
    
    return false;
}

bool GameRoom::removePlayerByPeer(ENetPeer* peer) {
    auto it = std::find_if(players.begin(), players.end(),
        [peer](const PlayerInRoom& p) { return p.peer == peer; });
    
    if (it != players.end()) {
        std::string username = it->username;
        return removePlayer(username);
    }
    
    return false;
}

PlayerInRoom* GameRoom::getPlayer(const std::string& username) {
    auto it = std::find_if(players.begin(), players.end(),
        [&username](const PlayerInRoom& p) { return p.username == username; });
    
    return (it != players.end()) ? &(*it) : nullptr;
}

PlayerInRoom* GameRoom::getPlayerByPeer(ENetPeer* peer) {
    auto it = std::find_if(players.begin(), players.end(),
        [peer](const PlayerInRoom& p) { return p.peer == peer; });
    
    return (it != players.end()) ? &(*it) : nullptr;
}

bool GameRoom::isHost(const std::string& username) const {
    return hostUsername == username;
}

bool GameRoom::transferHost(const std::string& newHost) {
    // Check if new host is in the room
    auto it = std::find_if(players.begin(), players.end(),
        [&newHost](const PlayerInRoom& p) { return p.username == newHost; });
    
    if (it != players.end()) {
        hostUsername = newHost;
        updateActivity();
        return true;
    }
    
    return false;
}

bool GameRoom::setPlayerReady(const std::string& username, bool ready) {
    PlayerInRoom* player = getPlayer(username);
    if (player) {
        player->isReady = ready;
        updateActivity();
        return true;
    }
    return false;
}

bool GameRoom::areAllPlayersReady() const {
    if (players.empty()) {
        return false;
    }
    
    for (const auto& player : players) {
        if (!player.isReady) {
            return false;
        }
    }
    
    return true;
}

int GameRoom::getReadyPlayerCount() const {
    int count = 0;
    for (const auto& player : players) {
        if (player.isReady) {
            count++;
        }
    }
    return count;
}

bool GameRoom::assignPlayerToTeam(const std::string& username, int team) {
    if (gameMode != GameMode::TEAM_BATTLE) {
        return false;
    }
    
    if (team < 0 || team > 1) {
        return false;
    }
    
    PlayerInRoom* player = getPlayer(username);
    if (player) {
        player->team = team;
        updateActivity();
        return true;
    }
    
    return false;
}

int GameRoom::getTeamPlayerCount(int team) const {
    if (team < 0 || team > 1) {
        return 0;
    }
    
    int count = 0;
    for (const auto& player : players) {
        if (player.team == team) {
            count++;
        }
    }
    return count;
}

bool GameRoom::canStart() const {
    // Need at least 2 players
    if (players.size() < 2) {
        return false;
    }
    
    // Room must be in WAITING status
    if (status != RoomStatus::WAITING) {
        return false;
    }
    
    // All players must be ready
    if (!areAllPlayersReady()) {
        return false;
    }
    
    // For team mode, need at least 1 player per team
    if (gameMode == GameMode::TEAM_BATTLE) {
        if (getTeamPlayerCount(0) == 0 || getTeamPlayerCount(1) == 0) {
            return false;
        }
    }
    
    return true;
}

bool GameRoom::validatePassword(const std::string& pass) const {
    if (password.empty()) {
        return true;  // No password required
    }
    return password == pass;
}

void GameRoom::updateActivity() {
    lastActivity = std::chrono::system_clock::now();
}

bool GameRoom::isInactive(int timeoutSeconds) const {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity);
    return elapsed.count() > timeoutSeconds;
}

GameRoom::RoomInfo GameRoom::getRoomInfo() const {
    RoomInfo info;
    info.roomId = roomId;
    strncpy(info.roomName, roomName.c_str(), sizeof(info.roomName) - 1);
    info.roomName[sizeof(info.roomName) - 1] = '\0';
    strncpy(info.hostUsername, hostUsername.c_str(), sizeof(info.hostUsername) - 1);
    info.hostUsername[sizeof(info.hostUsername) - 1] = '\0';
    info.currentPlayers = players.size();
    info.maxPlayers = maxPlayers;
    info.gameMode = static_cast<int>(gameMode);
    info.mapId = mapId;
    info.hasPassword = !password.empty();
    info.status = static_cast<int>(status);
    return info;
}

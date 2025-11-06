#include "RoomManager.h"
#include <algorithm>
#include <iostream>

RoomManager::RoomManager() : nextRoomId(1) {
}

RoomManager::~RoomManager() {
    clearAllRooms();
}

int RoomManager::createRoom(const std::string& roomName, const std::string& hostUsername,
                            const std::string& password, int maxPlayers, GameMode gameMode, int mapId) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    // Validate max players
    if (maxPlayers < 2 || maxPlayers > 8) {
        return -1;
    }
    
    // Check if room name is taken (optional, allow duplicate names for now)
    // if (isRoomNameTaken(roomName)) {
    //     return -1;
    // }
    
    int roomId = nextRoomId++;
    auto room = std::make_shared<GameRoom>(roomId, roomName, hostUsername, password, maxPlayers, gameMode, mapId);
    rooms[roomId] = room;
    
    std::cout << "Room created: ID=" << roomId << ", Name=" << roomName 
              << ", Host=" << hostUsername << std::endl;
    
    return roomId;
}

bool RoomManager::deleteRoom(int roomId) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    auto it = rooms.find(roomId);
    if (it != rooms.end()) {
        std::cout << "Room deleted: ID=" << roomId << std::endl;
        rooms.erase(it);
        return true;
    }
    
    return false;
}

void RoomManager::deleteEmptyRooms() {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    std::vector<int> toDelete;
    for (const auto& pair : rooms) {
        if (pair.second->isEmpty()) {
            toDelete.push_back(pair.first);
        }
    }
    
    for (int roomId : toDelete) {
        std::cout << "Deleting empty room: ID=" << roomId << std::endl;
        rooms.erase(roomId);
    }
}

void RoomManager::deleteInactiveRooms(int timeoutSeconds) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    std::vector<int> toDelete;
    for (const auto& pair : rooms) {
        if (pair.second->isInactive(timeoutSeconds)) {
            toDelete.push_back(pair.first);
        }
    }
    
    for (int roomId : toDelete) {
        std::cout << "Deleting inactive room: ID=" << roomId << std::endl;
        rooms.erase(roomId);
    }
}

std::shared_ptr<GameRoom> RoomManager::getRoom(int roomId) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    auto it = rooms.find(roomId);
    if (it != rooms.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<GameRoom>> RoomManager::getAllRooms() {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    std::vector<std::shared_ptr<GameRoom>> result;
    result.reserve(rooms.size());
    
    for (const auto& pair : rooms) {
        result.push_back(pair.second);
    }
    
    return result;
}

std::vector<std::shared_ptr<GameRoom>> RoomManager::getAvailableRooms() {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    std::vector<std::shared_ptr<GameRoom>> result;
    
    for (const auto& pair : rooms) {
        if (!pair.second->isFull() && pair.second->getStatus() == RoomStatus::WAITING) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

std::vector<GameRoom::RoomInfo> RoomManager::getAllRoomInfo() {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    std::vector<GameRoom::RoomInfo> result;
    result.reserve(rooms.size());
    
    for (const auto& pair : rooms) {
        result.push_back(pair.second->getRoomInfo());
    }
    
    return result;
}

std::vector<GameRoom::RoomInfo> RoomManager::getAvailableRoomInfo() {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    std::vector<GameRoom::RoomInfo> result;
    
    for (const auto& pair : rooms) {
        if (!pair.second->isFull() && pair.second->getStatus() == RoomStatus::WAITING) {
            result.push_back(pair.second->getRoomInfo());
        }
    }
    
    return result;
}

std::shared_ptr<GameRoom> RoomManager::findRoomByHost(const std::string& hostUsername) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    for (const auto& pair : rooms) {
        if (pair.second->getHostUsername() == hostUsername) {
            return pair.second;
        }
    }
    
    return nullptr;
}

std::shared_ptr<GameRoom> RoomManager::findRoomWithPlayer(const std::string& username) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    for (const auto& pair : rooms) {
        if (pair.second->getPlayer(username) != nullptr) {
            return pair.second;
        }
    }
    
    return nullptr;
}

std::shared_ptr<GameRoom> RoomManager::findRoomByPeer(ENetPeer* peer) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    for (const auto& pair : rooms) {
        if (pair.second->getPlayerByPeer(peer) != nullptr) {
            return pair.second;
        }
    }
    
    return nullptr;
}

bool RoomManager::addPlayerToRoom(int roomId, const std::string& username, 
                                  ENetPeer* peer, const std::string& password) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    auto it = rooms.find(roomId);
    if (it == rooms.end()) {
        return false;
    }
    
    auto room = it->second;
    
    // Check if room is full
    if (room->isFull()) {
        return false;
    }
    
    // Check if room is in game
    if (room->getStatus() != RoomStatus::WAITING) {
        return false;
    }
    
    // Validate password
    if (!room->validatePassword(password)) {
        return false;
    }
    
    // Check if player is already in another room
    for (const auto& pair : rooms) {
        if (pair.second->getPlayer(username) != nullptr) {
            return false;  // Player already in a room
        }
    }
    
    return room->addPlayer(username, peer);
}

bool RoomManager::removePlayerFromRoom(int roomId, const std::string& username) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    auto it = rooms.find(roomId);
    if (it != rooms.end()) {
        return it->second->removePlayer(username);
    }
    
    return false;
}

bool RoomManager::removePlayerFromAllRooms(const std::string& username) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    for (const auto& pair : rooms) {
        if (pair.second->removePlayer(username)) {
            return true;
        }
    }
    
    return false;
}

bool RoomManager::removePlayerByPeer(ENetPeer* peer) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    for (const auto& pair : rooms) {
        if (pair.second->removePlayerByPeer(peer)) {
            return true;
        }
    }
    
    return false;
}

int RoomManager::getRoomCount() const {
    std::lock_guard<std::mutex> lock(roomsMutex);
    return rooms.size();
}

int RoomManager::getActiveRoomCount() const {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    int count = 0;
    for (const auto& pair : rooms) {
        if (!pair.second->isEmpty()) {
            count++;
        }
    }
    return count;
}

int RoomManager::getTotalPlayers() const {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    int count = 0;
    for (const auto& pair : rooms) {
        count += pair.second->getCurrentPlayerCount();
    }
    return count;
}

bool RoomManager::isRoomNameTaken(const std::string& roomName) const {
    // Note: lock is already held by caller in createRoom
    for (const auto& pair : rooms) {
        if (pair.second->getRoomName() == roomName) {
            return true;
        }
    }
    return false;
}

bool RoomManager::isPlayerInAnyRoom(const std::string& username) const {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    for (const auto& pair : rooms) {
        if (pair.second->getPlayer(username) != nullptr) {
            return true;
        }
    }
    return false;
}

void RoomManager::clearAllRooms() {
    std::lock_guard<std::mutex> lock(roomsMutex);
    rooms.clear();
    std::cout << "All rooms cleared" << std::endl;
}

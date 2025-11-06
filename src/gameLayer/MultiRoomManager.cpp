#include "MultiRoomManager.h"
#include "serverClient.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <ctime>

MultiRoomManager::MultiRoomManager() {
    rooms = std::make_unique<RoomSlot[]>(MAX_ROOMS);
    cleanupStaleLocks();  // Clean up any old lock files from crashed processes
    initializeRooms();
}

MultiRoomManager::~MultiRoomManager() {
    // Stop all active rooms and clean up locks
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            stopRoom(i);
        }
    }
}

void MultiRoomManager::initializeRooms() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].slotId = i;
        rooms[i].port = BASE_PORT + i;  // 7778, 7779, 7780
        rooms[i].active = false;
        rooms[i].roomName = "";
        rooms[i].hostName = "";
        rooms[i].maxPlayers = 4;
        rooms[i].currentPlayers = 0;
    }
}

int MultiRoomManager::createRoom(const std::string& roomName, 
                                  const std::string& hostName,
                                  int maxPlayers) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    // Find an available slot
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) {
            int port = rooms[i].port;
            
            // Check if port is already locked by another process (file-based)
            if (isPortLockedByFile(port)) {
                std::cout << "MultiRoomManager: Port " << port << " is locked by another process, skipping slot " << i << std::endl;
                continue;  // Try next slot
            }
            
            // Check if port is in use by this process
            if (isServerRunning(port)) {
                std::cout << "MultiRoomManager: Port " << port << " is already in use in this process, skipping slot " << i << std::endl;
                continue;  // Try next slot
            }
            
            // Lock the port using file-based lock
            if (!lockPort(port, roomName)) {
                std::cout << "MultiRoomManager: Failed to lock port " << port << ", skipping slot " << i << std::endl;
                continue;  // Try next slot
            }
            
            // Configure room
            rooms[i].active = true;
            rooms[i].roomName = roomName;
            rooms[i].hostName = hostName;
            rooms[i].maxPlayers = maxPlayers;
            rooms[i].currentPlayers = 1;  // Host counts as player
            
            // Start server thread
            rooms[i].serverThread = std::make_unique<std::thread>([port]() {
                serverFunction(port);
            });
            
            std::cout << "MultiRoomManager: Created room '" << roomName 
                      << "' in slot " << i << " on port " << port << std::endl;
            
            return i;  // Return slot ID
        }
    }
    
    std::cout << "MultiRoomManager: No available room slots (max " << MAX_ROOMS << " rooms)" << std::endl;
    return -1;  // No available slots
}

void MultiRoomManager::stopRoom(int slotId) {
    if (slotId < 0 || slotId >= MAX_ROOMS) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    if (rooms[slotId].active) {
        int port = rooms[slotId].port;
        
        std::cout << "MultiRoomManager: Stopping room '" << rooms[slotId].roomName 
                  << "' in slot " << slotId << " on port " << port << std::endl;
        
        // Close server on specific port
        closeServerByPort(port);
        
        // Wait for thread to finish
        if (rooms[slotId].serverThread && rooms[slotId].serverThread->joinable()) {
            rooms[slotId].serverThread->join();
        }
        rooms[slotId].serverThread.reset();
        
        // Unlock the port (remove lock file)
        unlockPort(port);
        
        // Mark slot as inactive
        rooms[slotId].active = false;
        rooms[slotId].roomName = "";
        rooms[slotId].hostName = "";
        rooms[slotId].currentPlayers = 0;
        
        std::cout << "MultiRoomManager: Room in slot " << slotId << " stopped." << std::endl;
    }
}

std::vector<RoomInfo> MultiRoomManager::getActiveRooms() {
    std::lock_guard<std::mutex> lock(roomsMutex);
    std::vector<RoomInfo> activeRooms;
    
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            RoomInfo info;
            info.slotId = rooms[i].slotId;
            info.port = rooms[i].port;
            info.active = rooms[i].active.load();
            info.roomName = rooms[i].roomName;
            info.hostName = rooms[i].hostName;
            info.maxPlayers = rooms[i].maxPlayers;
            info.currentPlayers = rooms[i].currentPlayers.load();
            activeRooms.push_back(info);
        }
    }
    
    return activeRooms;
}

void MultiRoomManager::updateRoomPlayers(int slotId, int playerCount) {
    if (slotId < 0 || slotId >= MAX_ROOMS) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    if (rooms[slotId].active) {
        rooms[slotId].currentPlayers = playerCount;
    }
}

RoomInfo MultiRoomManager::getRoomInfo(int slotId) {
    if (slotId < 0 || slotId >= MAX_ROOMS) {
        return RoomInfo{-1, 0, false, "", "", 0, 0};
    }
    
    std::lock_guard<std::mutex> lock(roomsMutex);
    
    if (rooms[slotId].active) {
        RoomInfo info;
        info.slotId = rooms[slotId].slotId;
        info.port = rooms[slotId].port;
        info.active = rooms[slotId].active.load();
        info.roomName = rooms[slotId].roomName;
        info.hostName = rooms[slotId].hostName;
        info.maxPlayers = rooms[slotId].maxPlayers;
        info.currentPlayers = rooms[slotId].currentPlayers.load();
        return info;
    }
    
    return RoomInfo{-1, 0, false, "", "", 0, 0};
}

bool MultiRoomManager::hasActiveRooms() const {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            return true;
        }
    }
    return false;
}

int MultiRoomManager::getActiveRoomCount() const {
    int count = 0;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            count++;
        }
    }
    return count;
}

// Port lock file management
std::string MultiRoomManager::getPortLockFilePath(int port) const {
    std::stringstream ss;
    ss << "/tmp/game_port_" << port << ".lock";
    return ss.str();
}

bool MultiRoomManager::isPortLockedByFile(int port) const {
    std::string lockFile = getPortLockFilePath(port);
    std::ifstream file(lockFile);
    
    if (!file.good()) {
        return false;  // Lock file doesn't exist
    }
    
    // Read PID from lock file
    int pid = 0;
    file >> pid;
    file.close();
    
    // Check if the process is still running
    if (pid > 0 && kill(pid, 0) == 0) {
        return true;  // Process is running, port is locked
    }
    
    // Process is dead, remove stale lock file
    unlink(lockFile.c_str());
    return false;
}

bool MultiRoomManager::lockPort(int port, const std::string& roomName) {
    if (isPortLockedByFile(port)) {
        return false;  // Port is already locked
    }
    
    std::string lockFile = getPortLockFilePath(port);
    std::ofstream file(lockFile);
    
    if (!file.is_open()) {
        std::cerr << "Failed to create lock file: " << lockFile << std::endl;
        return false;
    }
    
    // Write PID and room info to lock file
    file << getpid() << std::endl;
    file << roomName << std::endl;
    file << time(nullptr) << std::endl;
    file.close();
    
    std::cout << "MultiRoomManager: Locked port " << port << " (PID: " << getpid() << ")" << std::endl;
    return true;
}

void MultiRoomManager::unlockPort(int port) {
    std::string lockFile = getPortLockFilePath(port);
    if (unlink(lockFile.c_str()) == 0) {
        std::cout << "MultiRoomManager: Unlocked port " << port << std::endl;
    }
}

void MultiRoomManager::cleanupStaleLocks() {
    std::cout << "MultiRoomManager: Cleaning up stale lock files..." << std::endl;
    
    for (int i = 0; i < MAX_ROOMS; i++) {
        int port = BASE_PORT + i;
        
        if (isPortLockedByFile(port)) {
            // Lock is valid, port is in use
            std::cout << "MultiRoomManager: Port " << port << " is locked by another process" << std::endl;
        } else {
            // Lock was stale or doesn't exist
            std::cout << "MultiRoomManager: Port " << port << " is available" << std::endl;
        }
    }
}

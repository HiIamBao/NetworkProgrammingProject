#include "SessionManager.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

SessionManager::SessionManager(AccountManager* accMgr) 
    : accountManager(accMgr) {
}

SessionManager::~SessionManager() {
    std::lock_guard<std::mutex> lock(sessionMutex);
    sessions.clear();
    usernameSessions.clear();
    peerSessions.clear();
}

std::string SessionManager::generateToken() {
    // Generate a random token
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (int i = 0; i < 32; i++) {
        ss << std::setw(2) << dis(gen);
    }
    
    return ss.str();
}

std::string SessionManager::login(const std::string& username, const std::string& password, ENetPeer* peer) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    if (!accountManager) {
        std::cerr << "Account manager not initialized" << std::endl;
        return "";
    }
    
    // Validate credentials
    if (!accountManager->validateCredentials(username, password)) {
        std::cerr << "Invalid credentials for user: " << username << std::endl;
        return "";
    }
    
    // Check if user is already logged in
    auto existingIt = usernameSessions.find(username);
    if (existingIt != usernameSessions.end()) {
        std::string oldToken = existingIt->second;
        auto sessionIt = sessions.find(oldToken);
        if (sessionIt != sessions.end()) {
            // Force logout old session
            if (sessionIt->second.peer) {
                peerSessions.erase(sessionIt->second.peer);
            }
            sessions.erase(sessionIt);
        }
        usernameSessions.erase(existingIt);
        std::cout << "Forced logout of existing session for: " << username << std::endl;
    }
    
    // Create new session
    std::string token = generateToken();
    
    Session session;
    session.sessionToken = token;
    session.username = username;
    session.peer = peer;
    session.loginTime = std::chrono::system_clock::now();
    session.lastActivity = std::chrono::system_clock::now();
    session.isActive = true;
    session.roomId = -1;
    
    sessions[token] = session;
    usernameSessions[username] = token;
    if (peer) {
        peerSessions[peer] = token;
    }
    
    std::cout << "User logged in: " << username << " with token: " << token.substr(0, 8) << "..." << std::endl;
    
    return token;
}

bool SessionManager::logout(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    auto it = sessions.find(token);
    if (it == sessions.end()) {
        return false;
    }
    
    std::string username = it->second.username;
    ENetPeer* peer = it->second.peer;
    
    // Remove all references
    usernameSessions.erase(username);
    if (peer) {
        peerSessions.erase(peer);
    }
    sessions.erase(it);
    
    std::cout << "User logged out: " << username << std::endl;
    
    return true;
}

bool SessionManager::logoutByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    auto it = usernameSessions.find(username);
    if (it == usernameSessions.end()) {
        return false;
    }
    
    std::string token = it->second;
    auto sessionIt = sessions.find(token);
    if (sessionIt != sessions.end()) {
        if (sessionIt->second.peer) {
            peerSessions.erase(sessionIt->second.peer);
        }
        sessions.erase(sessionIt);
    }
    
    usernameSessions.erase(it);
    
    std::cout << "User logged out: " << username << std::endl;
    
    return true;
}

bool SessionManager::logoutByPeer(ENetPeer* peer) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    if (!peer) {
        return false;
    }
    
    auto it = peerSessions.find(peer);
    if (it == peerSessions.end()) {
        return false;
    }
    
    std::string token = it->second;
    auto sessionIt = sessions.find(token);
    if (sessionIt != sessions.end()) {
        std::string username = sessionIt->second.username;
        usernameSessions.erase(username);
        sessions.erase(sessionIt);
        std::cout << "User logged out by peer: " << username << std::endl;
    }
    
    peerSessions.erase(it);
    
    return true;
}

bool SessionManager::validateSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    auto it = sessions.find(token);
    if (it == sessions.end()) {
        return false;
    }
    
    if (!it->second.isActive) {
        return false;
    }
    
    if (isSessionExpired(it->second)) {
        // Session expired, remove it
        std::string username = it->second.username;
        ENetPeer* peer = it->second.peer;
        
        usernameSessions.erase(username);
        if (peer) {
            peerSessions.erase(peer);
        }
        sessions.erase(it);
        
        std::cout << "Session expired for user: " << username << std::endl;
        return false;
    }
    
    return true;
}

void SessionManager::updateActivity(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    auto it = sessions.find(token);
    if (it != sessions.end()) {
        it->second.lastActivity = std::chrono::system_clock::now();
    }
}

std::string SessionManager::getUsername(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    auto it = sessions.find(token);
    if (it != sessions.end()) {
        return it->second.username;
    }
    
    return "";
}

std::string SessionManager::getTokenByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    auto it = usernameSessions.find(username);
    if (it != usernameSessions.end()) {
        return it->second;
    }
    
    return "";
}

std::string SessionManager::getTokenByPeer(ENetPeer* peer) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    if (!peer) {
        return "";
    }
    
    auto it = peerSessions.find(peer);
    if (it != peerSessions.end()) {
        return it->second;
    }
    
    return "";
}

Session* SessionManager::getSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    auto it = sessions.find(token);
    if (it != sessions.end()) {
        return &(it->second);
    }
    
    return nullptr;
}

Session* SessionManager::getSessionByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    auto it = usernameSessions.find(username);
    if (it != usernameSessions.end()) {
        auto sessionIt = sessions.find(it->second);
        if (sessionIt != sessions.end()) {
            return &(sessionIt->second);
        }
    }
    
    return nullptr;
}

Session* SessionManager::getSessionByPeer(ENetPeer* peer) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    if (!peer) {
        return nullptr;
    }
    
    auto it = peerSessions.find(peer);
    if (it != peerSessions.end()) {
        auto sessionIt = sessions.find(it->second);
        if (sessionIt != sessions.end()) {
            return &(sessionIt->second);
        }
    }
    
    return nullptr;
}

bool SessionManager::setRoom(const std::string& token, int roomId) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    auto it = sessions.find(token);
    if (it != sessions.end()) {
        it->second.roomId = roomId;
        return true;
    }
    
    return false;
}

int SessionManager::getRoom(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    auto it = sessions.find(token);
    if (it != sessions.end()) {
        return it->second.roomId;
    }
    
    return -1;
}

bool SessionManager::isSessionExpired(const Session& session) {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - session.lastActivity);
    
    return duration.count() > SESSION_TIMEOUT;
}

void SessionManager::cleanExpiredSessions() {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    std::vector<std::string> tokensToRemove;
    
    for (auto& pair : sessions) {
        if (isSessionExpired(pair.second)) {
            tokensToRemove.push_back(pair.first);
        }
    }
    
    for (const auto& token : tokensToRemove) {
        auto it = sessions.find(token);
        if (it != sessions.end()) {
            std::string username = it->second.username;
            ENetPeer* peer = it->second.peer;
            
            usernameSessions.erase(username);
            if (peer) {
                peerSessions.erase(peer);
            }
            sessions.erase(it);
            
            std::cout << "Cleaned expired session for user: " << username << std::endl;
        }
    }
}

int SessionManager::getActiveSessionCount() {
    std::lock_guard<std::mutex> lock(sessionMutex);
    return sessions.size();
}

std::vector<std::string> SessionManager::getActivePlayers() {
    std::lock_guard<std::mutex> lock(sessionMutex);
    
    std::vector<std::string> players;
    for (const auto& pair : sessions) {
        if (pair.second.isActive) {
            players.push_back(pair.second.username);
        }
    }
    
    return players;
}

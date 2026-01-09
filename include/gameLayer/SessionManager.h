#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <enet/enet.h>
#include "AccountManager.h"

struct Session {
    std::string sessionToken;
    std::string username;
    ENetPeer* peer;
    std::chrono::system_clock::time_point loginTime;
    std::chrono::system_clock::time_point lastActivity;
    bool isActive;
    int roomId;
    
    Session() : peer(nullptr), isActive(false), roomId(-1) {}
};

class SessionManager {
private:
    std::unordered_map<std::string, Session> sessions;           // token -> session
    std::unordered_map<std::string, std::string> usernameSessions; // username -> token
    std::unordered_map<ENetPeer*, std::string> peerSessions;     // peer -> token
    AccountManager* accountManager;
    std::mutex sessionMutex;
    
    const int SESSION_TIMEOUT = 3600; // 1 hour in seconds
    
public:
    SessionManager(AccountManager* accMgr);
    ~SessionManager();
    
    // Login/Logout
    std::string login(const std::string& username, const std::string& password, ENetPeer* peer);
    bool logout(const std::string& token);
    bool logoutByUsername(const std::string& username);
    bool logoutByPeer(ENetPeer* peer);
    
    // Force disconnect (session control - another login)
    void forceKickSession(const std::string& username, const std::string& reason);
    
    // Session validation
    bool validateSession(const std::string& token);
    void updateActivity(const std::string& token);
    
    // Session info
    std::string getUsername(const std::string& token);
    std::string getTokenByUsername(const std::string& username);
    std::string getTokenByPeer(ENetPeer* peer);
    Session* getSession(const std::string& token);
    Session* getSessionByUsername(const std::string& username);
    Session* getSessionByPeer(ENetPeer* peer);
    
    // Room management
    bool setRoom(const std::string& token, int roomId);
    int getRoom(const std::string& token);
    
    // Maintenance
    void cleanExpiredSessions();
    int getActiveSessionCount();
    std::vector<std::string> getActivePlayers();
    
private:
    std::string generateToken();
    bool isSessionExpired(const Session& session);
};

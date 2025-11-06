#pragma once

#include "AccountManager.h"
#include "SessionManager.h"
#include "packet.h"
#include <enet/enet.h>
#include <string>
#include <vector>
#include <cstring>

// Helper structures for packet data
struct RegisterData {
    char username[32];
    char password[64];
    char email[64];
};

struct LoginData {
    char username[32];
    char password[64];
};

struct LoginResponseData {
    bool success;
    char token[128];
    char message[256];
    
    // Account info
    int level;
    int totalScore;
    int gamesPlayed;
    int gamesWon;
    float winRate;
};

struct AccountInfoData {
    char username[32];
    int level;
    int totalScore;
    int gamesPlayed;
    int gamesWon;
    float winRate;
    int ranking;
};

class AuthenticationHandler {
private:
    AccountManager* accountManager;
    SessionManager* sessionManager;
    
public:
    AuthenticationHandler(AccountManager* accMgr, SessionManager* sessMgr);
    
    // Handle authentication packets
    void handleRegisterRequest(ENetPeer* peer, const char* data, size_t dataSize);
    void handleLoginRequest(ENetPeer* peer, const char* data, size_t dataSize);
    void handleLogoutRequest(ENetPeer* peer, const char* data, size_t dataSize);
    void handleAccountInfoRequest(ENetPeer* peer, const char* data, size_t dataSize);
    void handleLeaderboardRequest(ENetPeer* peer);
    
    // Validate session for game actions
    bool validateSessionForAction(ENetPeer* peer, std::string& username);
    
    // Handle disconnection
    void handlePeerDisconnect(ENetPeer* peer);
    
private:
    void sendRegisterResponse(ENetPeer* peer, bool success, const char* message);
    void sendLoginResponse(ENetPeer* peer, bool success, const char* token, const char* message, Account* account = nullptr);
    void sendAccountInfo(ENetPeer* peer, const Account& account);
    void sendLeaderboard(ENetPeer* peer, const std::vector<Account>& topPlayers);
};

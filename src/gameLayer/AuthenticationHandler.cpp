#include "AuthenticationHandler.h"
#include <iostream>
#include <cstring>

AuthenticationHandler::AuthenticationHandler(AccountManager* accMgr, SessionManager* sessMgr)
    : accountManager(accMgr), sessionManager(sessMgr) {
}

void AuthenticationHandler::handleRegisterRequest(ENetPeer* peer, const char* data, size_t dataSize) {
    if (dataSize < sizeof(RegisterData)) {
        sendRegisterResponse(peer, false, "Invalid registration data");
        return;
    }
    
    RegisterData regData;
    memcpy(&regData, data, sizeof(RegisterData));
    
    std::string username(regData.username);
    std::string password(regData.password);
    std::string email(regData.email);
    
    std::cout << "Registration attempt: " << username << " (" << email << ")" << std::endl;
    
    // Validate input
    if (username.empty() || password.empty() || email.empty()) {
        sendRegisterResponse(peer, false, "All fields are required");
        return;
    }
    
    if (username.length() < 3 || username.length() > 20) {
        sendRegisterResponse(peer, false, "Username must be 3-20 characters");
        return;
    }
    
    if (password.length() < 6) {
        sendRegisterResponse(peer, false, "Password must be at least 6 characters");
        return;
    }
    
    // Check if username exists
    if (accountManager->accountExists(username)) {
        sendRegisterResponse(peer, false, "Username already exists");
        return;
    }
    
    // Check if email exists
    if (accountManager->emailExists(email)) {
        sendRegisterResponse(peer, false, "Email already registered");
        return;
    }
    
    // Register account
    if (accountManager->registerAccount(username, password, email)) {
        sendRegisterResponse(peer, true, "Account created successfully");
        std::cout << "Account registered: " << username << std::endl;
    } else {
        sendRegisterResponse(peer, false, "Registration failed. Please try again");
    }
}

void AuthenticationHandler::handleLoginRequest(ENetPeer* peer, const char* data, size_t dataSize) {
    if (dataSize < sizeof(LoginData)) {
        sendLoginResponse(peer, false, "", "Invalid login data");
        return;
    }
    
    LoginData loginData;
    memcpy(&loginData, data, sizeof(LoginData));
    
    std::string username(loginData.username);
    std::string password(loginData.password);
    
    std::cout << "Login attempt: " << username << std::endl;
    
    // Attempt login
    std::string token = sessionManager->login(username, password, peer);
    
    if (!token.empty()) {
        // Get account info
        Account* account = accountManager->getAccount(username);
        sendLoginResponse(peer, true, token.c_str(), "Login successful", account);
        std::cout << "Login successful: " << username << std::endl;
    } else {
        sendLoginResponse(peer, false, "", "Invalid username or password");
        std::cout << "Login failed: " << username << std::endl;
    }
}

void AuthenticationHandler::handleLogoutRequest(ENetPeer* peer, const char* data, size_t dataSize) {
    std::string token = sessionManager->getTokenByPeer(peer);
    
    if (!token.empty()) {
        std::string username = sessionManager->getUsername(token);
        sessionManager->logout(token);
        std::cout << "User logged out: " << username << std::endl;
    }
    
    Packet response;
    response.header = headerLogoutResponse;
    sendPacket(peer, response, nullptr, 0, true, 0);
}

void AuthenticationHandler::handleAccountInfoRequest(ENetPeer* peer, const char* data, size_t dataSize) {
    std::string token = sessionManager->getTokenByPeer(peer);
    
    if (token.empty() || !sessionManager->validateSession(token)) {
        std::cerr << "Invalid session for account info request" << std::endl;
        return;
    }
    
    std::string username = sessionManager->getUsername(token);
    Account* account = accountManager->getAccount(username);
    
    if (account) {
        sendAccountInfo(peer, *account);
        sessionManager->updateActivity(token);
    }
}

void AuthenticationHandler::handleLeaderboardRequest(ENetPeer* peer) {
    std::string token = sessionManager->getTokenByPeer(peer);
    
    if (token.empty() || !sessionManager->validateSession(token)) {
        std::cerr << "Invalid session for leaderboard request" << std::endl;
        return;
    }
    
    std::vector<Account> topPlayers = accountManager->getTopPlayers(100);
    sendLeaderboard(peer, topPlayers);
    sessionManager->updateActivity(token);
}

bool AuthenticationHandler::validateSessionForAction(ENetPeer* peer, std::string& username) {
    std::string token = sessionManager->getTokenByPeer(peer);
    
    if (token.empty()) {
        return false;
    }
    
    if (!sessionManager->validateSession(token)) {
        return false;
    }
    
    username = sessionManager->getUsername(token);
    sessionManager->updateActivity(token);
    
    return !username.empty();
}

void AuthenticationHandler::handlePeerDisconnect(ENetPeer* peer) {
    sessionManager->logoutByPeer(peer);
}

void AuthenticationHandler::sendRegisterResponse(ENetPeer* peer, bool success, const char* message) {
    struct ResponseData {
        bool success;
        char message[256];
    } response;
    
    response.success = success;
    strncpy(response.message, message, sizeof(response.message) - 1);
    response.message[sizeof(response.message) - 1] = '\0';
    
    Packet packet;
    packet.header = headerRegisterResponse;
    
    sendPacket(peer, packet, reinterpret_cast<char*>(&response), sizeof(response), true, 0);
}

void AuthenticationHandler::sendLoginResponse(ENetPeer* peer, bool success, const char* token, const char* message, Account* account) {
    LoginResponseData response;
    response.success = success;
    
    strncpy(response.token, token, sizeof(response.token) - 1);
    response.token[sizeof(response.token) - 1] = '\0';
    
    strncpy(response.message, message, sizeof(response.message) - 1);
    response.message[sizeof(response.message) - 1] = '\0';
    
    if (account) {
        response.level = account->level;
        response.totalScore = account->totalScore;
        response.gamesPlayed = account->gamesPlayed;
        response.gamesWon = account->gamesWon;
        response.winRate = account->winRate;
    } else {
        response.level = 0;
        response.totalScore = 0;
        response.gamesPlayed = 0;
        response.gamesWon = 0;
        response.winRate = 0.0f;
    }
    
    Packet packet;
    packet.header = headerLoginResponse;
    
    sendPacket(peer, packet, reinterpret_cast<char*>(&response), sizeof(response), true, 0);
}

void AuthenticationHandler::sendAccountInfo(ENetPeer* peer, const Account& account) {
    AccountInfoData data;
    
    strncpy(data.username, account.username.c_str(), sizeof(data.username) - 1);
    data.username[sizeof(data.username) - 1] = '\0';
    
    data.level = account.level;
    data.totalScore = account.totalScore;
    data.gamesPlayed = account.gamesPlayed;
    data.gamesWon = account.gamesWon;
    data.winRate = account.winRate;
    data.ranking = account.ranking;
    
    Packet packet;
    packet.header = headerAccountInfo;
    
    sendPacket(peer, packet, reinterpret_cast<char*>(&data), sizeof(data), true, 0);
}

void AuthenticationHandler::sendLeaderboard(ENetPeer* peer, const std::vector<Account>& topPlayers) {
    // Calculate total size needed
    int playerCount = std::min(static_cast<int>(topPlayers.size()), 100);
    size_t dataSize = sizeof(int) + (playerCount * sizeof(AccountInfoData));
    
    char* buffer = new char[dataSize];
    
    // Write player count
    memcpy(buffer, &playerCount, sizeof(int));
    
    // Write player data
    for (int i = 0; i < playerCount; i++) {
        AccountInfoData data;
        
        strncpy(data.username, topPlayers[i].username.c_str(), sizeof(data.username) - 1);
        data.username[sizeof(data.username) - 1] = '\0';
        
        data.level = topPlayers[i].level;
        data.totalScore = topPlayers[i].totalScore;
        data.gamesPlayed = topPlayers[i].gamesPlayed;
        data.gamesWon = topPlayers[i].gamesWon;
        data.winRate = topPlayers[i].winRate;
        data.ranking = topPlayers[i].ranking;
        
        memcpy(buffer + sizeof(int) + (i * sizeof(AccountInfoData)), &data, sizeof(AccountInfoData));
    }
    
    Packet packet;
    packet.header = headerLeaderboard;
    
    sendPacket(peer, packet, buffer, dataSize, true, 0);
    
    delete[] buffer;
}

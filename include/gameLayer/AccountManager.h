#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <sqlite3.h>

struct Account {
    int id;
    std::string username;
    std::string passwordHash;
    std::string email;
    int level;
    int totalScore;
    int gamesPlayed;
    int gamesWon;
    float winRate;
    int ranking;
    std::string createdAt;
    
    Account() : id(0), level(1), totalScore(0), gamesPlayed(0), gamesWon(0), winRate(0.0f), ranking(0) {}
};

class AccountManager {
private:
    sqlite3* db;
    std::unordered_map<std::string, Account> cachedAccounts;
    std::mutex accountMutex;
    bool initialized;
    
public:
    AccountManager();
    ~AccountManager();
    
    // Initialization
    bool initialize(const std::string& dbPath);
    void shutdown();
    
    // Account operations
    bool registerAccount(const std::string& username, const std::string& password, const std::string& email);
    bool validateCredentials(const std::string& username, const std::string& password);
    Account* getAccount(const std::string& username);
    bool updateAccount(const Account& account);
    bool deleteAccount(const std::string& username);
    
    // Statistics
    bool updateStats(const std::string& username, int scoreChange, bool won);
    bool updateRanking(const std::string& username, int newRanking);
    
    // Query
    bool accountExists(const std::string& username);
    bool emailExists(const std::string& email);
    std::vector<Account> getTopPlayers(int limit = 100);
    
private:
    std::string hashPassword(const std::string& password);
    bool createTables();
    bool loadAccountFromDB(const std::string& username, Account& account);
    void cacheAccount(const Account& account);
};

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <sqlite3.h>

struct MatchPlayerStats {
    int playerId;
    std::string playerName;
    int kills;          // For Deathmatch
    int roundsSurvived; // For Horde Defense (waves survived)
    int damageDealt;    // For Boss Fight and Horde Defense
};

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
    
    // Deathmatch Stats
    int deathmatchTotalScore;
    int deathmatchGamesPlayed;
    int deathmatchGamesWon;
    
    // Horde Defense Stats
    int hordeDefenseTotalScore;   // Legacy field (for compatibility)
    int hordeDefenseGamesPlayed;
    int hordeDefenseGamesWon;
    int hordeDefenseBestWave;     // Best wave reached (for ranking)
    int hordeDefenseTotalDamage;  // Total damage dealt (tie-breaker)
    
    // Boss Fight Stats
    int bossFightTotalScore;
    int bossFightGamesPlayed;
    int bossFightGamesWon;
    
    Account()
        : id(0),
          level(1),
          totalScore(0),
          gamesPlayed(0),
          gamesWon(0),
          winRate(0.0f),
          ranking(0),
          deathmatchTotalScore(0),
          deathmatchGamesPlayed(0),
          deathmatchGamesWon(0),
          hordeDefenseTotalScore(0),
          hordeDefenseGamesPlayed(0),
          hordeDefenseGamesWon(0),
          hordeDefenseBestWave(0),
          hordeDefenseTotalDamage(0),
          bossFightTotalScore(0),
          bossFightGamesPlayed(0),
          bossFightGamesWon(0) {}
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
    
    // Match End Statistics
    bool recordDeathmatchMatchEnd(const std::vector<MatchPlayerStats>& stats);
    bool recordHordeDefenseMatchEnd(const std::vector<MatchPlayerStats>& stats);
    bool recordBossFightMatchEnd(const std::vector<MatchPlayerStats>& stats, int bossStageLevel);
    
    // Query
    bool accountExists(const std::string& username);
    bool emailExists(const std::string& email);
    
    // Session control (database-level for local login)
    bool isAccountLoggedIn(const std::string& username);
    bool setAccountLoggedIn(const std::string& username, bool loggedIn);
    void clearAllLoggedInFlags();  // Call on startup to clean stale sessions
    
    std::vector<Account> getTopPlayers(int limit = 100);
    std::vector<Account> getTopPlayersForMode(int mode, int limit = 100);
    
    // Admin operations
    bool setAllLevels(int level);
    
private:
    std::string hashPassword(const std::string& password);
    bool createTables();
    bool loadAccountFromDB(const std::string& username, Account& account);
    void cacheAccount(const Account& account);
};

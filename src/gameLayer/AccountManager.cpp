#include "AccountManager.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <iostream>

// Simple SHA256 implementation (you may want to use a proper crypto library)
#include <openssl/sha.h>

AccountManager::AccountManager() : db(nullptr), initialized(false) {
}

AccountManager::~AccountManager() {
    shutdown();
}

bool AccountManager::initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(accountMutex);
    
    if (initialized) {
        return true;
    }
    
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    if (!createTables()) {
        std::cerr << "Failed to create tables" << std::endl;
        sqlite3_close(db);
        db = nullptr;
        return false;
    }
    
    initialized = true;
    std::cout << "Account Manager initialized successfully" << std::endl;
    return true;
}

void AccountManager::shutdown() {
    std::lock_guard<std::mutex> lock(accountMutex);
    
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
    
    cachedAccounts.clear();
    initialized = false;
}

bool AccountManager::createTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS accounts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL COLLATE NOCASE,
            password_hash TEXT NOT NULL,
            email TEXT UNIQUE NOT NULL COLLATE NOCASE,
            level INTEGER DEFAULT 69,
            total_score INTEGER DEFAULT 0,
            games_played INTEGER DEFAULT 100,
            games_won INTEGER DEFAULT 0,
            win_rate REAL DEFAULT 0.0,
            ranking INTEGER DEFAULT 0,
            deathmatch_total_score INTEGER DEFAULT 0,
            deathmatch_games_played INTEGER DEFAULT 100,
            deathmatch_games_won INTEGER DEFAULT 0,

            horde_defense_total_score INTEGER DEFAULT 0,
            horde_defense_games_played INTEGER DEFAULT 100,
            horde_defense_games_won INTEGER DEFAULT 0,
          
            boss_fight_total_score INTEGER DEFAULT 0,
            boss_fight_games_played INTEGER DEFAULT 100,
            boss_fight_games_won INTEGER DEFAULT 0,
    
       
         
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE INDEX IF NOT EXISTS idx_username ON accounts(username);
        CREATE INDEX IF NOT EXISTS idx_email ON accounts(email);
        CREATE INDEX IF NOT EXISTS idx_ranking ON accounts(ranking DESC);
    )";
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    // Try to add new columns if they don't exist (migrations)
    // We ignore errors assuming they might already exist
    // Try to add new columns if they don't exist (migrations)
    // We execute them one by one so if one fails (already exists), the others still run.
    const char* migrations[] = {
        "ALTER TABLE accounts ADD COLUMN deathmatch_total_score INTEGER DEFAULT 0;",
        "ALTER TABLE accounts ADD COLUMN deathmatch_games_played INTEGER DEFAULT 100;",
        "ALTER TABLE accounts ADD COLUMN deathmatch_games_won INTEGER DEFAULT 0;",
        "ALTER TABLE accounts ADD COLUMN horde_defense_total_score INTEGER DEFAULT 0;",
        "ALTER TABLE accounts ADD COLUMN horde_defense_games_played INTEGER DEFAULT 100;",
        "ALTER TABLE accounts ADD COLUMN horde_defense_games_won INTEGER DEFAULT 0;",
        "ALTER TABLE accounts ADD COLUMN horde_defense_best_wave INTEGER DEFAULT 0;",
        "ALTER TABLE accounts ADD COLUMN horde_defense_total_damage INTEGER DEFAULT 0;",
        "ALTER TABLE accounts ADD COLUMN boss_fight_total_score INTEGER DEFAULT 0;",
        "ALTER TABLE accounts ADD COLUMN boss_fight_games_played INTEGER DEFAULT 100;",
        "ALTER TABLE accounts ADD COLUMN boss_fight_games_won INTEGER DEFAULT 0;"
    };

    for (const char* migration : migrations) {
        rc = sqlite3_exec(db, migration, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK && errMsg) {
            // Check if error is due to duplicate column
            // SQLite error message for this is usually "duplicate column name: ..."
            // We'll just ignore it as per requirement to be idempotent
            sqlite3_free(errMsg);
            errMsg = nullptr;
        }
    }
    
    return true;
}

std::string AccountManager::hashPassword(const std::string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), password.length(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

bool AccountManager::registerAccount(const std::string& username, const std::string& password, const std::string& email) {
    std::lock_guard<std::mutex> lock(accountMutex);
    
    if (!initialized) {
        std::cerr << "Account Manager not initialized" << std::endl;
        return false;
    }
    
    // Validate input
    if (username.empty() || password.empty() || email.empty()) {
        std::cerr << "Invalid input: empty fields" << std::endl;
        return false;
    }
    
    if (username.length() < 3 || username.length() > 20) {
        std::cerr << "Username must be between 3 and 20 characters" << std::endl;
        return false;
    }
    
    if (password.length() < 6) {
        std::cerr << "Password must be at least 6 characters" << std::endl;
        return false;
    }
    
    std::string passwordHash = hashPassword(password);
    
    const char* sql = "INSERT INTO accounts (username, password_hash, email) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_TRANSIENT);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc == SQLITE_DONE) {
        std::cout << "Account registered successfully: " << username << std::endl;
        return true;
    } else {
        std::cerr << "Failed to register account: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
}

bool AccountManager::validateCredentials(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(accountMutex);
    
    if (!initialized) {
        return false;
    }
    
    std::string passwordHash = hashPassword(password);
    
    const char* sql = "SELECT id FROM accounts WHERE username = ? COLLATE NOCASE AND password_hash = ?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_ROW;
}

Account* AccountManager::getAccount(const std::string& username) {
    std::lock_guard<std::mutex> lock(accountMutex);
    
    if (!initialized) {
        return nullptr;
    }
    
    // Check cache first
    auto it = cachedAccounts.find(username);
    if (it != cachedAccounts.end()) {
        return &(it->second);
    }
    
    // Load from database
    Account account;
    if (loadAccountFromDB(username, account)) {
        cacheAccount(account);
        return &cachedAccounts[username];
    }
    
    return nullptr;
}

bool AccountManager::loadAccountFromDB(const std::string& username, Account& account) {
    const char* sql = "SELECT id, username, email, level, total_score, games_played, games_won, win_rate, ranking, "
                      "deathmatch_total_score, deathmatch_games_played, deathmatch_games_won, "
                      "horde_defense_total_score, horde_defense_games_played, horde_defense_games_won, "
                      "horde_defense_best_wave, horde_defense_total_damage, "
                      "boss_fight_total_score, boss_fight_games_played, boss_fight_games_won, "
                      "created_at FROM accounts WHERE username = ? COLLATE NOCASE";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        account.id = sqlite3_column_int(stmt, 0);
        account.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        account.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        account.level = sqlite3_column_int(stmt, 3);
        account.totalScore = sqlite3_column_int(stmt, 4);
        account.gamesPlayed = sqlite3_column_int(stmt, 5);
        account.gamesWon = sqlite3_column_int(stmt, 6);
        account.winRate = static_cast<float>(sqlite3_column_double(stmt, 7));
        account.ranking = sqlite3_column_int(stmt, 8);
        
        account.deathmatchTotalScore = sqlite3_column_int(stmt, 9);
        account.deathmatchGamesPlayed = sqlite3_column_int(stmt, 10);
        account.deathmatchGamesWon = sqlite3_column_int(stmt, 11);
        
        account.hordeDefenseTotalScore = sqlite3_column_int(stmt, 12);
        account.hordeDefenseGamesPlayed = sqlite3_column_int(stmt, 13);
        account.hordeDefenseGamesWon = sqlite3_column_int(stmt, 14);
        account.hordeDefenseBestWave = sqlite3_column_int(stmt, 15);
        account.hordeDefenseTotalDamage = sqlite3_column_int(stmt, 16);
        
        account.bossFightTotalScore = sqlite3_column_int(stmt, 17);
        account.bossFightGamesPlayed = sqlite3_column_int(stmt, 18);
        account.bossFightGamesWon = sqlite3_column_int(stmt, 19);
        
        account.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 20));
        
        sqlite3_finalize(stmt);
        return true;
    }
    
    sqlite3_finalize(stmt);
    return false;
}


void AccountManager::cacheAccount(const Account& account) {
    cachedAccounts[account.username] = account;
}

bool AccountManager::updateAccount(const Account& account) {
    std::lock_guard<std::mutex> lock(accountMutex);
    
    if (!initialized) {
        return false;
    }
    
    const char* sql = "UPDATE accounts SET level = ?, total_score = ?, games_played = ?, games_won = ?, win_rate = ?, ranking = ?, "
                      "deathmatch_total_score = ?, deathmatch_games_played = ?, deathmatch_games_won = ?, "
                      "horde_defense_total_score = ?, horde_defense_games_played = ?, horde_defense_games_won = ?, "
                      "horde_defense_best_wave = ?, horde_defense_total_damage = ?, "
                      "boss_fight_total_score = ?, boss_fight_games_played = ?, boss_fight_games_won = ? "
                      "WHERE username = ?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, account.level);
    sqlite3_bind_int(stmt, 2, account.totalScore);
    sqlite3_bind_int(stmt, 3, account.gamesPlayed);
    sqlite3_bind_int(stmt, 4, account.gamesWon);
    sqlite3_bind_double(stmt, 5, account.winRate);
    sqlite3_bind_int(stmt, 6, account.ranking);
    
    sqlite3_bind_int(stmt, 7, account.deathmatchTotalScore);
    sqlite3_bind_int(stmt, 8, account.deathmatchGamesPlayed);
    sqlite3_bind_int(stmt, 9, account.deathmatchGamesWon);
    
    sqlite3_bind_int(stmt, 10, account.hordeDefenseTotalScore);
    sqlite3_bind_int(stmt, 11, account.hordeDefenseGamesPlayed);
    sqlite3_bind_int(stmt, 12, account.hordeDefenseGamesWon);
    sqlite3_bind_int(stmt, 13, account.hordeDefenseBestWave);
    sqlite3_bind_int(stmt, 14, account.hordeDefenseTotalDamage);
    
    sqlite3_bind_int(stmt, 15, account.bossFightTotalScore);
    sqlite3_bind_int(stmt, 16, account.bossFightGamesPlayed);
    sqlite3_bind_int(stmt, 17, account.bossFightGamesWon);
    
    sqlite3_bind_text(stmt, 18, account.username.c_str(), -1, SQLITE_TRANSIENT);

    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc == SQLITE_DONE) {
        cacheAccount(account);
        return true;
    }
    
    return false;
}

bool AccountManager::updateStats(const std::string& username, int scoreChange, bool won) {
    Account* account = getAccount(username);
    if (!account) {
        return false;
    }
    
    // Update general stats
    account->totalScore += scoreChange;
    account->gamesPlayed++;
    if (won) {
        account->gamesWon++;
    }
    
    // Update deathmatch-specific stats (scoreChange = kills in deathmatch)
    account->deathmatchTotalScore += scoreChange;
    account->deathmatchGamesPlayed++;
    if (won) {
        account->deathmatchGamesWon++;
    }
    
    if (account->gamesPlayed > 0) {
        account->winRate = static_cast<float>(account->gamesWon) / static_cast<float>(account->gamesPlayed);
    }
    
    // Level up based on total score
    account->level = 1 + (account->totalScore / 1000);
    
    return updateAccount(*account);
}

bool AccountManager::updateRanking(const std::string& username, int newRanking) {
    Account* account = getAccount(username);
    if (!account) {
        return false;
    }
    
    account->ranking = newRanking;
    return updateAccount(*account);
}

bool AccountManager::accountExists(const std::string& username) {
    std::lock_guard<std::mutex> lock(accountMutex);
    
    if (!initialized) {
        return false;
    }
    
    const char* sql = "SELECT id FROM accounts WHERE username = ? COLLATE NOCASE";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_ROW;
}

bool AccountManager::emailExists(const std::string& email) {
    std::lock_guard<std::mutex> lock(accountMutex);
    
    if (!initialized) {
        return false;
    }
    
    const char* sql = "SELECT id FROM accounts WHERE email = ? COLLATE NOCASE";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_ROW;
}

std::vector<Account> AccountManager::getTopPlayers(int limit) {
    std::lock_guard<std::mutex> lock(accountMutex);
    std::vector<Account> topPlayers;
    
    if (!initialized) {
        return topPlayers;
    }
    
    const char* sql = "SELECT id, username, email, level, total_score, games_played, games_won, win_rate, ranking, "
                      "deathmatch_total_score, deathmatch_games_played, deathmatch_games_won, "
                      "horde_defense_total_score, horde_defense_games_played, horde_defense_games_won, "
                      "horde_defense_best_wave, horde_defense_total_damage, "
                      "boss_fight_total_score, boss_fight_games_played, boss_fight_games_won, "
                      "created_at FROM accounts ORDER BY ranking DESC, total_score DESC LIMIT ?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return topPlayers;
    }
    
    sqlite3_bind_int(stmt, 1, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Account account;
        account.id = sqlite3_column_int(stmt, 0);
        account.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        account.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        account.level = sqlite3_column_int(stmt, 3);
        account.totalScore = sqlite3_column_int(stmt, 4);
        account.gamesPlayed = sqlite3_column_int(stmt, 5);
        account.gamesWon = sqlite3_column_int(stmt, 6);
        account.winRate = static_cast<float>(sqlite3_column_double(stmt, 7));
        account.ranking = sqlite3_column_int(stmt, 8);
        
        account.deathmatchTotalScore = sqlite3_column_int(stmt, 9);
        account.deathmatchGamesPlayed = sqlite3_column_int(stmt, 10);
        account.deathmatchGamesWon = sqlite3_column_int(stmt, 11);
        
        account.hordeDefenseTotalScore = sqlite3_column_int(stmt, 12);
        account.hordeDefenseGamesPlayed = sqlite3_column_int(stmt, 13);
        account.hordeDefenseGamesWon = sqlite3_column_int(stmt, 14);
        account.hordeDefenseBestWave = sqlite3_column_int(stmt, 15);
        account.hordeDefenseTotalDamage = sqlite3_column_int(stmt, 16);
        
        account.bossFightTotalScore = sqlite3_column_int(stmt, 17);
        account.bossFightGamesPlayed = sqlite3_column_int(stmt, 18);
        account.bossFightGamesWon = sqlite3_column_int(stmt, 19);
        
        account.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 20));
        
        topPlayers.push_back(account);
    }
    
    sqlite3_finalize(stmt);
    return topPlayers;
}


std::vector<Account> AccountManager::getTopPlayersForMode(int mode, int limit) {
    std::lock_guard<std::mutex> lock(accountMutex);
    std::vector<Account> topPlayers;
    
    if (!initialized) {
        return topPlayers;
    }
    
    std::string orderBy;
    switch(mode) {
        case 0: // Deathmatch - Sort by Kills (Total Score)
            orderBy = "deathmatch_total_score DESC";
            break;
        case 1: // Horde Defense - Sort by Best Wave (primary), Total Damage (secondary)
            orderBy = "horde_defense_best_wave DESC, horde_defense_total_damage DESC";
            break;
        case 2: // Boss Fight - Sort by Total Score
            orderBy = "boss_fight_total_score DESC";
            break;
        default:
            orderBy = "total_score DESC"; 
            break;
    }
    
    std::string sqlStr = "SELECT id, username, email, level, total_score, games_played, games_won, win_rate, ranking, "
                      "deathmatch_total_score, deathmatch_games_played, deathmatch_games_won, "
                      "horde_defense_total_score, horde_defense_games_played, horde_defense_games_won, "
                      "horde_defense_best_wave, horde_defense_total_damage, "
                      "boss_fight_total_score, boss_fight_games_played, boss_fight_games_won, "
                      "created_at FROM accounts ORDER BY " + orderBy + ", username ASC LIMIT ?";
                      
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sqlStr.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return topPlayers;
    }
    
    sqlite3_bind_int(stmt, 1, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Account account;
        account.id = sqlite3_column_int(stmt, 0);
        account.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        account.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        account.level = sqlite3_column_int(stmt, 3);
        account.totalScore = sqlite3_column_int(stmt, 4);
        account.gamesPlayed = sqlite3_column_int(stmt, 5);
        account.gamesWon = sqlite3_column_int(stmt, 6);
        account.winRate = static_cast<float>(sqlite3_column_double(stmt, 7));
        account.ranking = sqlite3_column_int(stmt, 8);
        
        account.deathmatchTotalScore = sqlite3_column_int(stmt, 9);
        account.deathmatchGamesPlayed = sqlite3_column_int(stmt, 10);
        account.deathmatchGamesWon = sqlite3_column_int(stmt, 11);
        
        account.hordeDefenseTotalScore = sqlite3_column_int(stmt, 12);
        account.hordeDefenseGamesPlayed = sqlite3_column_int(stmt, 13);
        account.hordeDefenseGamesWon = sqlite3_column_int(stmt, 14);
        account.hordeDefenseBestWave = sqlite3_column_int(stmt, 15);
        account.hordeDefenseTotalDamage = sqlite3_column_int(stmt, 16);
        
        account.bossFightTotalScore = sqlite3_column_int(stmt, 17);
        account.bossFightGamesPlayed = sqlite3_column_int(stmt, 18);
        account.bossFightGamesWon = sqlite3_column_int(stmt, 19);
        
        account.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 20));
        
        topPlayers.push_back(account);
    }
    
    sqlite3_finalize(stmt);
    return topPlayers;
}


// ============================================================================
// MATCH END STATISTICS
// ============================================================================

bool AccountManager::recordDeathmatchMatchEnd(const std::vector<MatchPlayerStats>& stats) {
    if (stats.empty()) return false;
    
    int maxKills = -1;
    // Determine winner based on kills
    for (const auto& p : stats) {
        if (p.kills > maxKills) {
            maxKills = p.kills;
        }
    }
    
    bool success = true;
    for (const auto& p : stats) {
        Account* account = getAccount(p.playerName);
        if (account) {
            // Update Deathmatch stats
            account->deathmatchGamesPlayed++;
            account->deathmatchTotalScore += p.kills; // Accumulate TOTAL KILLS
            
            // Note: Total score can be kills for now or accumulative
            // User requirement: "DeathMatchMatchEnd(playerstats[])" -> calculate winning player based on kills
            // "After all packet, increase game_played by 1 for that gamemode"
            
            bool isWinner = (p.kills == maxKills && maxKills > 0);
            if (isWinner) {
                account->deathmatchGamesWon++;
            }
            
            // Also update global stats
            account->gamesPlayed++;
            if (isWinner) account->gamesWon++;
            if (account->gamesPlayed > 0) {
                account->winRate = (float)account->gamesWon / account->gamesPlayed;
            }
            
            if (!updateAccount(*account)) {
                success = false;
            }
        }
    }
    return success;
}

bool AccountManager::recordHordeDefenseMatchEnd(const std::vector<MatchPlayerStats>& stats) {
    if (stats.empty()) return false;
    
    // Check if any player reached wave 20 (victory)
    bool wave20Reached = false;
    for (const auto& p : stats) {
        if (p.roundsSurvived >= 5) {  // TEMP: Changed from 20 for testing winner screen
            wave20Reached = true;
            break;
        }
    }
    
    bool success = true;
    for (const auto& p : stats) {
        Account* account = getAccount(p.playerName);
        if (account) {
            // Update Horde Defense Stats
            account->hordeDefenseGamesPlayed++;
            
            // Update best wave (keep the maximum)
            if (p.roundsSurvived > account->hordeDefenseBestWave) {
                account->hordeDefenseBestWave = p.roundsSurvived;
            }
            
            // Accumulate total damage dealt
            account->hordeDefenseTotalDamage += p.damageDealt;
            
            // Also update legacy score field with wave count for backwards compatibility
            account->hordeDefenseTotalScore = account->hordeDefenseBestWave;
            
            if (wave20Reached) {
                account->hordeDefenseGamesWon++;
            }
            
            // Update global stats
            account->gamesPlayed++;
            if (wave20Reached) account->gamesWon++;
            if (account->gamesPlayed > 0) {
                account->winRate = (float)account->gamesWon / account->gamesPlayed;
            }
            
            std::cout << "[HordeDefense] Recorded match for " << p.playerName 
                      << ": Wave " << p.roundsSurvived 
                      << ", Damage " << p.damageDealt 
                      << ", Best Wave " << account->hordeDefenseBestWave 
                      << ", Total Damage " << account->hordeDefenseTotalDamage << std::endl;
            
            if (!updateAccount(*account)) {
                success = false;
            }
        }
    }
    return success;
}

bool AccountManager::recordBossFightMatchEnd(const std::vector<MatchPlayerStats>& stats, int bossStageLevel) {
    if (stats.empty()) return false;
    
    int maxTotalScore = -1;
    std::unordered_map<std::string, int> playerScores;
    
    // Calculate scores and find winner
    for (const auto& p : stats) {
        int score = p.damageDealt * bossStageLevel;
        playerScores[p.playerName] = score;
        
        if (score > maxTotalScore) {
            maxTotalScore = score;
        }
    }
    
    bool success = true;
    for (const auto& p : stats) {
        Account* account = getAccount(p.playerName);
        if (account) {
            int score = playerScores[p.playerName];
            
            // Update Boss Fight stats
            account->bossFightGamesPlayed++;
            account->bossFightTotalScore += score;
            
            bool isWinner = (score == maxTotalScore && maxTotalScore > 0);
            if (isWinner) {
                account->bossFightGamesWon++;
            }
            
            // Update global stats
            account->gamesPlayed++;
            account->totalScore += score; // Add match score to global score too
            if (isWinner) account->gamesWon++;
            
             if (account->gamesPlayed > 0) {
                account->winRate = (float)account->gamesWon / account->gamesPlayed;
            }
            
            // Recalculate level based on total score logic (from updateStats)
            account->level = 1 + (account->totalScore / 1000);

            if (!updateAccount(*account)) {
                success = false;
            }
        }
    }
    return success;
}

bool AccountManager::deleteAccount(const std::string& username) {
    std::lock_guard<std::mutex> lock(accountMutex);
    
    if (!initialized) {
        return false;
    }
    
    const char* sql = "DELETE FROM accounts WHERE username = ?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc == SQLITE_DONE) {
        cachedAccounts.erase(username);
        return true;
    }
    
    return false;
}

bool AccountManager::setAllLevels(int level) {
    std::lock_guard<std::mutex> lock(accountMutex);
    
    if (!initialized) {
        std::cerr << "Account Manager not initialized" << std::endl;
        return false;
    }
    
    const char* sql = "UPDATE accounts SET level = ?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, level);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc == SQLITE_DONE) {
        // Clear cache since levels have changed
        cachedAccounts.clear();
        std::cout << "All account levels set to " << level << std::endl;
        return true;
    } else {
        std::cerr << "Failed to update levels: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
}

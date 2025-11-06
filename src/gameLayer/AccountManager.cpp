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
            level INTEGER DEFAULT 1,
            total_score INTEGER DEFAULT 0,
            games_played INTEGER DEFAULT 0,
            games_won INTEGER DEFAULT 0,
            win_rate REAL DEFAULT 0.0,
            ranking INTEGER DEFAULT 0,
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
    const char* sql = "SELECT id, username, email, level, total_score, games_played, games_won, win_rate, ranking, created_at FROM accounts WHERE username = ? COLLATE NOCASE";
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
        account.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        
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
    
    const char* sql = "UPDATE accounts SET level = ?, total_score = ?, games_played = ?, games_won = ?, win_rate = ?, ranking = ? WHERE username = ?";
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
    sqlite3_bind_text(stmt, 7, account.username.c_str(), -1, SQLITE_TRANSIENT);
    
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
    
    account->totalScore += scoreChange;
    account->gamesPlayed++;
    if (won) {
        account->gamesWon++;
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
    
    const char* sql = "SELECT id, username, email, level, total_score, games_played, games_won, win_rate, ranking, created_at FROM accounts ORDER BY ranking DESC, total_score DESC LIMIT ?";
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
        account.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        
        topPlayers.push_back(account);
    }
    
    sqlite3_finalize(stmt);
    return topPlayers;
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

#include "include/gameLayer/AccountManager.h"
#include <iostream>
#include <vector>

int main() {
    AccountManager accountMgr;
    
    // Path to your main database
    const std::string dbPath = "./resources/game_accounts.db";
    
    std::cout << "Initializing AccountManager with DB: " << dbPath << std::endl;
    if (!accountMgr.initialize(dbPath)) {
        std::cerr << "Failed to initialize AccountManager. Make sure the path is correct." << std::endl;
        return 1;
    }
    
    std::string targetPlayer = "hung1fps";
    
    // 1. Ensure the account exists (Optional, but good for safety)
    if (!accountMgr.accountExists(targetPlayer)) {
        std::cout << "User '" << targetPlayer << "' does not exist. Creating it now..." << std::endl;
        // Register with a default password if not exists
        if (!accountMgr.registerAccount(targetPlayer, "password123", "hung1fps@test.com")) {
            std::cerr << "Failed to register user." << std::endl;
            return 1;
        }
    }
    
    std::cout << "Select simulation mode:" << std::endl;
    std::cout << "1. Deathmatch (100 Kills, Win)" << std::endl;
    std::cout << "2. Horde Defense (69 Rounds Survived, Win)" << std::endl;
    std::cout << "3. Boss Fight (5000 Damage, Level 2 Boss, Win)" << std::endl;
    std::cout << "4. All" << std::endl;
    int choice;
    std::cin >> choice;

    bool runDM = (choice == 1 || choice == 4);
    bool runHD = (choice == 2 || choice == 4);
    bool runBF = (choice == 3 || choice == 4);

    if (runDM) {
        std::cout << "\n--- Simulating Deathmatch for: " << targetPlayer << " ---" << std::endl;
        
        std::vector<MatchPlayerStats> matches;
        MatchPlayerStats p1;
        p1.playerId = 0;
        p1.playerName = targetPlayer;
        p1.kills = 100;
        p1.roundsSurvived = 0;
        p1.damageDealt = 0;
        
        matches.push_back(p1);
        
        if (accountMgr.recordDeathmatchMatchEnd(matches)) {
            std::cout << "✅ Successfully updated DM stats." << std::endl;
        } else {
            std::cerr << "❌ Failed to update DM stats." << std::endl;
        }
    }

    if (runHD) {
        std::cout << "\n--- Simulating Horde Defense for: " << targetPlayer << " ---" << std::endl;
        
        std::vector<MatchPlayerStats> matches;
        MatchPlayerStats p1;
        p1.playerId = 0;
        p1.playerName = targetPlayer;
        p1.kills = 0;
        p1.roundsSurvived = 69; // Triggers win condition (>= 20)
        p1.damageDealt = 0;
        
        matches.push_back(p1);
        
        if (accountMgr.recordHordeDefenseMatchEnd(matches)) {
            std::cout << "✅ Successfully updated HD stats." << std::endl;
            std::cout << "   - Set Rounds Survived to 69" << std::endl;
            std::cout << "   - Incremented Games Played" << std::endl;
            std::cout << "   - Incremented Games Won (Waves >= 20)" << std::endl;
        } else {
            std::cerr << "❌ Failed to update HD stats." << std::endl;
        }
    }

    if (runBF) {
        std::cout << "\n--- Simulating Boss Fight for: " << targetPlayer << " ---" << std::endl;
        
        std::vector<MatchPlayerStats> matches;
        MatchPlayerStats p1;
        p1.playerId = 0;
        p1.playerName = targetPlayer;
        p1.kills = 0;
        p1.roundsSurvived = 0;
        p1.damageDealt = 5000;
        
        matches.push_back(p1);
        
        int bossLevel = 2;
        if (accountMgr.recordBossFightMatchEnd(matches, bossLevel)) {
            std::cout << "✅ Successfully updated BF stats." << std::endl;
            std::cout << "   - Damage: 5000, Boss Level: 2 -> Score: 10000" << std::endl;
            std::cout << "   - Incremented Games Played" << std::endl;
            std::cout << "   - Incremented Games Won" << std::endl;
        } else {
            std::cerr << "❌ Failed to update BF stats." << std::endl;
        }
    }

    // Verify
    Account* acc = accountMgr.getAccount(targetPlayer);
    if (acc) {
        std::cout << "\n=== Current Stats for " << targetPlayer << " ===" << std::endl;
        std::cout << "   DM Score (Kills): " << acc->deathmatchTotalScore << std::endl;
        std::cout << "   DM Wins:          " << acc->deathmatchGamesWon << std::endl;
        std::cout << "   HD Wins:          " << acc->hordeDefenseGamesWon << std::endl;
        std::cout << "   HD Played:        " << acc->hordeDefenseGamesPlayed << std::endl;
        std::cout << "   BF Score:         " << acc->bossFightTotalScore << std::endl;
        std::cout << "   BF Wins:          " << acc->bossFightGamesWon << std::endl;
        std::cout << "   Total Score:      " << acc->totalScore << std::endl;
    }

    return 0;
}

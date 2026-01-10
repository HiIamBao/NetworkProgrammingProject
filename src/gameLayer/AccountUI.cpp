#include "AccountUI.h"
#include "platformInput.h"
#include "imgui.h"
#include "gameLayer/Ui.h"
#include <cstring>
#include <cctype>
#include <algorithm>

AccountUI::AccountUI(AccountManager* accMgr, SessionManager* sessMgr)
    : accountManager(accMgr), sessionManager(sessMgr), 
      currentState(UIState::MAIN_MENU), isLoggedIn(false),
      messageTimer(0.0f), currentAccount(nullptr),
      leaderboardRefreshTimer(0.0f) {
    clearInputs();
    statusColor = UIColors::White;
}

AccountUI::~AccountUI() {
}

void AccountUI::render(gl2d::Renderer2D& renderer, gl2d::Font& font, const Textures& textures, float deltaTime) {
    // Check for force disconnect (session control - another login kicked us)
    extern std::string g_forceDisconnectReason;
    extern bool g_wasForceDisconnected;
    
    if (g_wasForceDisconnected) {
        // Reset our login state
        isLoggedIn = false;
        currentUsername.clear();
        sessionToken.clear();
        currentAccount = nullptr;
        
        // Show the disconnect message
        showMessage(g_forceDisconnectReason, UIColors::Error);
        
        // Reset to main menu (login screen)
        setState(UIState::MAIN_MENU);
        
        // Clear the flag
        g_wasForceDisconnected = false;
        g_forceDisconnectReason.clear();
    }
    
    // Save current camera and reset to default for UI rendering
    auto savedCamera = renderer.currentCamera;
    renderer.currentCamera.setDefault();
    
    // Render fullscreen background image first
    auto bgBox = Ui::Box()
        .xLeft(0.0f)
        .yTop(0.0f)
        .xDimensionPixels(renderer.windowW)
        .yDimensionPixels(renderer.windowH);
    
    renderer.renderRectangle(bgBox, {1.0f, 1.0f, 1.0f, 1.0f}, {}, 0.f, textures.accountBackground);
    
    // Restore camera (though glui will reset it anyway)
    renderer.currentCamera = savedCamera;
    
    // Update message timer
    if (messageTimer > 0.0f) {
        messageTimer -= deltaTime;
        if (messageTimer <= 0.0f) {
            statusMessage.clear();
        }
    }
    
    // Update leaderboard refresh timer
    if (currentState == UIState::LEADERBOARD) {
        leaderboardRefreshTimer += deltaTime;
        if (leaderboardRefreshTimer > 5.0f) {
            refreshLeaderboard();
            leaderboardRefreshTimer = 0.0f;
        }
    }
    
    // Render based on current state
    switch (currentState) {
        case UIState::MAIN_MENU:
            renderMainMenu(renderer, font);
            break;
        case UIState::LOGIN_SCREEN:
            renderLoginScreen(renderer, font);
            break;
        case UIState::REGISTER_SCREEN:
            renderRegisterScreen(renderer, font);
            break;
        case UIState::ACCOUNT_INFO:
            renderAccountInfo(renderer, font);
            break;
        case UIState::LEADERBOARD:
            renderLeaderboard(renderer, font);
            break;
        case UIState::MATCH_MAKING:
            renderMatchMaking(renderer, font);
            break;
        case UIState::MATCH_SUMMARY:
            renderMatchSummary(renderer, font);
            break;
        default:
            break;
    }
}

void AccountUI::renderMatchMaking(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("===== MATCH MAKING =====", UIColors::Primary);
    glui::Space(20);

    // Max players selection (like RoomUI::renderCreateRoom)
    glui::Text("Max Players:", UIColors::White);
    const char* playerOptions[] = {"2", "4", "6", "8"};
    for (int i = 0; i < 4; i++) {
        char buttonLabel[64];
        if (mmSelectedMaxPlayersIdx == i) {
            snprintf(buttonLabel, sizeof(buttonLabel),
                     ">>> %s Players <<<##mm_maxp%d", playerOptions[i], i);
        } else {
            snprintf(buttonLabel, sizeof(buttonLabel),
                     "    %s Players    ##mm_maxp%d", playerOptions[i], i);
        }

        glm::vec4 btnColor = (mmSelectedMaxPlayersIdx == i)
                                 ? UIColors::Success
                                 : UIColors::Panel;
        if (glui::Button(buttonLabel, btnColor)) {
            mmSelectedMaxPlayersIdx = i;
        }
    }
    glui::Space(10);

    // Game mode selection (same 4 options as RoomUI)
    glui::Text("Game Mode:", UIColors::White);
    const char* modeOptions[] = {
        "Deathmatch (FFA)", "Team Battle", "Boss Fight", "Horde Defense"};
    for (int i = 0; i < 4; i++) {
        char buttonLabel[64];
        if (mmSelectedGameMode == i) {
            snprintf(buttonLabel, sizeof(buttonLabel),
                     ">>> %s <<<##mm_mode%d", modeOptions[i], i);
        } else {
            snprintf(buttonLabel, sizeof(buttonLabel),
                     "    %s    ##mm_mode%d", modeOptions[i], i);
        }

        glm::vec4 btnColor = (mmSelectedGameMode == i)
                                 ? UIColors::Success
                                 : UIColors::Panel;
        if (glui::Button(buttonLabel, btnColor)) {
            mmSelectedGameMode = i;
        }
    }
    glui::Space(20);

    if (glui::Button("Start Search", UIColors::Success)) {
        if (onMatchmakingRequest) {
            onMatchmakingRequest(true);
        }
    }

    if (glui::Button("Cancel Search", UIColors::Panel)) {
        if (onMatchmakingRequest) {
            onMatchmakingRequest(false);
        }
    }

    glui::Space(20);

    if (glui::Button("Back", UIColors::Panel)) {
        setState(UIState::MAIN_MENU);
    }
}

void AccountUI::renderMainMenu(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("HUST ARENA", UIColors::Primary);
    glui::Space(20);
    
    if (isLoggedIn) {
        // Show logged in user info
        std::string welcomeText = "Welcome, " + currentUsername + "!";
        glui::Text(welcomeText.c_str(), UIColors::Success);
        
        glui::Space(20);
        
        // Menu buttons for logged in user
        if (glui::Button("Match History", UIColors::Panel)) {
            refreshMatchHistory();
            setState(UIState::ACCOUNT_INFO);
        }
        
        if (glui::Button("View Leaderboard", UIColors::Panel)) {
            refreshLeaderboard();
            setState(UIState::LEADERBOARD);
        }
        
        glui::Space(10);
        
        if (glui::Button("Browse Rooms", UIColors::Success)) {
            setState(UIState::BROWSE_ROOMS);
        }

        // if (glui::Button("Match Making", UIColors::Success)) {
        //     setState(UIState::MATCH_MAKING);
        // }
        
        // if (glui::Button("Host Game", UIColors::Primary)) {
        //     setState(UIState::HOST_SERVER);
        // }
        
        // if (glui::Button("Join Game", UIColors::Primary)) {
        //     setState(UIState::JOIN_SERVER);
        // }
        
        glui::Space(20);
        
        if (glui::Button("Logout", UIColors::Error)) {
            logout();
        }
    } else {
        // Show login/register options
        glui::Text("Please login or create an account", UIColors::White);
        glui::Space(20);
        
        if (glui::Button("Login", UIColors::Primary)) {
            clearInputs();
            setState(UIState::LOGIN_SCREEN);
        }
        
        if (glui::Button("Register New Account", UIColors::Success)) {
            clearInputs();
            setState(UIState::REGISTER_SCREEN);
        }
        
        glui::Space(10);
        
        if (glui::Button("View Leaderboard", UIColors::Panel)) {
            refreshLeaderboard();
            setState(UIState::LEADERBOARD);
        }
    }
    
    glui::Space(20);
    
    // Display status message
    if (!statusMessage.empty()) {
        glui::Text(statusMessage.c_str(), statusColor);
    }
}

void AccountUI::renderLoginScreen(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("===== LOGIN =====", UIColors::Primary);
    glui::Space(20);
    
    glui::Text("Username:", UIColors::White);
    glui::Space(10);
    glui::InputText("##username_login", usernameInput, sizeof(usernameInput));
    
    glui::Space(10);
    
    glui::Text("Password:", UIColors::White);
    glui::Space(10);
    glui::InputText("##password_login", passwordInput, sizeof(passwordInput), UIColors::Panel);
    
    glui::Space(20);
    
    if (glui::Button("Login", UIColors::Primary)) {
        attemptLogin();
    }
    
    if (glui::Button("Back", UIColors::Panel)) {
        clearInputs();
        setState(UIState::MAIN_MENU);
    }
    
    glui::Space(20);
    
    if (!statusMessage.empty()) {
        glui::Text(statusMessage.c_str(), statusColor);
    }
}

void AccountUI::renderRegisterScreen(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("===== CREATE ACCOUNT =====", UIColors::Success);
    glui::Space(20);
    
    glui::Text("Username (3-20 characters):", UIColors::White);
    glui::InputText("##username_reg", usernameInput, sizeof(usernameInput));
    
    glui::Space(10);
    
    glui::Text("Email:", UIColors::White);
    glui::InputText("##email_reg", emailInput, sizeof(emailInput));
    
    glui::Space(10);
    
    glui::Text("Password (min 6 characters):", UIColors::White);
    glui::InputText("##password_reg", passwordInput, sizeof(passwordInput), UIColors::Panel);
    
    glui::Space(10);
    
    glui::Text("Confirm Password:", UIColors::White);
    glui::InputText("##confirm_password_reg", confirmPasswordInput, sizeof(confirmPasswordInput), UIColors::Panel);
    
    glui::Space(20);
    
    if (glui::Button("Register", UIColors::Success)) {
        attemptRegister();
    }
    
    if (glui::Button("Back", UIColors::Panel)) {
        clearInputs();
        setState(UIState::MAIN_MENU);
    }
    
    glui::Space(20);
    
    if (!statusMessage.empty()) {
        glui::Text(statusMessage.c_str(), statusColor);
    }
}

void AccountUI::renderAccountInfo(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("===== MATCH HISTORY =====", UIColors::Primary);
    glui::Space(10);
    
    // Mode tabs: Deathmatch, Horde Defense, Boss Fight
    const char* modeTabs[] = {"Deathmatch", "Horde Defense", "Boss Fight"};
    for (int i = 0; i < 3; i++) {
        char label[64];
        if (matchHistorySelectedMode == i) {
            snprintf(label, sizeof(label), ">>> %s <<<##mh_mode%d", modeTabs[i], i);
        } else {
            snprintf(label, sizeof(label), "    %s    ##mh_mode%d", modeTabs[i], i);
        }
        
        glm::vec4 color = (matchHistorySelectedMode == i) ? UIColors::Success : UIColors::Panel;
        if (glui::Button(label, color)) {
            matchHistorySelectedMode = i;
            refreshMatchHistory();
        }
        
        glui::SameLine();
    }
    glui::NewLine();
    glui::Space(10);
    
    // Display match history
    if (matchHistoryCache.empty()) {
        glui::Text("No matches found for this mode", UIColors::White);
    } else {
        char buffer[256];
        int displayCount = std::min(10, (int)matchHistoryCache.size());
        
        for (int i = 0; i < displayCount; i++) {
            const MatchRecord& record = matchHistoryCache[i];
            
            // Result color
            glm::vec4 resultColor = record.result == 1 ? UIColors::Success : UIColors::Error;
            const char* resultText = record.result == 1 ? "WIN" : "LOSS";
            
            // Format based on game mode
            if (record.gameMode == 0) {  // Deathmatch
                snprintf(buffer, sizeof(buffer), "[%s] %s - %d kills", resultText, record.playedAt.c_str(), record.score);
            } else if (record.gameMode == 1) {  // Horde Defense
                snprintf(buffer, sizeof(buffer), "[%s] %s - %d damage (%s)", resultText, record.playedAt.c_str(), record.score, record.extraData.c_str());
            } else {  // Boss Fight
                snprintf(buffer, sizeof(buffer), "[%s] %s - %d score (%s)", resultText, record.playedAt.c_str(), record.score, record.extraData.c_str());
            }
            
            glui::Text(buffer, resultColor);
            glui::Space(3);
        }
    }
    
    glui::Space(10);
    
    if (glui::Button("Refresh", UIColors::Primary)) {
        refreshMatchHistory();
    }
    
    if (glui::Button("Back", UIColors::Panel)) {
        setState(UIState::MAIN_MENU);
    }
}

void AccountUI::renderLeaderboard(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("===== LEADERBOARD =====", UIColors::Primary);
    glui::Space(10);
    
    // Game-mode tabs: Deathmatch, Horde Defense, Boss Fight
    const char* modeTabs[] = {"Deathmatch", "Horde Defense", "Boss Fight"};
    for (int i = 0; i < 3; i++) {
        char label[64];
        if (leaderboardSelectedMode == i) {
            snprintf(label, sizeof(label), ">>> %s <<<##lb_mode%d", modeTabs[i], i);
        } else {
            snprintf(label, sizeof(label), "    %s    ##lb_mode%d", modeTabs[i], i);
        }
        
        glm::vec4 color = (leaderboardSelectedMode == i) ? UIColors::Success : UIColors::Panel;
        if (glui::Button(label, color)) {
            leaderboardSelectedMode = i;
            refreshLeaderboard(); // Refresh immediately when tab changes
        }
        
        glui::SameLine(); // Render on one row
    }
    glui::NewLine();
    glui::Space(10);
    
    if (leaderboardCache.empty()) {
        glui::Text("No players found", UIColors::White);
    } else {
        char buffer[256];
        int displayCount = std::min(5, (int)leaderboardCache.size());
        
        for (int i = 0; i < displayCount; i++) {
            const Account& acc = leaderboardCache[i];
            
            glm::vec4 color = UIColors::White;
            if (i == 0) color = glm::vec4(1.0f, 0.84f, 0.0f, 1.0f); // Gold
            else if (i == 1) color = glm::vec4(0.75f, 0.75f, 0.75f, 1.0f); // Silver
            else if (i == 2) color = glm::vec4(0.8f, 0.5f, 0.2f, 1.0f); // Bronze
            
            // 1. If Selected mode is Death match, display the Name of player and the amount of kills ranking
            if (leaderboardSelectedMode == 0) {
                 sprintf(buffer, "%d. %s - Kills: %d", 
                    i + 1, acc.username.c_str(), acc.deathmatchTotalScore);
            }
            // 2. If selected mode is Horde defense, display the name with best wave and total damage
            else if (leaderboardSelectedMode == 1) {
                 sprintf(buffer, "%d. %s - Wave: %d | Damage: %d", 
                    i + 1, acc.username.c_str(), acc.hordeDefenseBestWave, acc.hordeDefenseTotalDamage);
            }
            // 3. If selected mode is Boss fight, display the name along side the total score
            else if (leaderboardSelectedMode == 2) {
                 sprintf(buffer, "%d. %s - Score: %d", 
                    i + 1, acc.username.c_str(), acc.bossFightTotalScore);
            }
            // Fallback
            else {
                sprintf(buffer, "%d. %s - Lvl %d - Score: %d", 
                    i + 1, acc.username.c_str(), acc.level, acc.totalScore);
            }

            glui::Text(buffer, color);
            glui::Space(5);
        }
    }
    
    glui::Space(10);
    
    if (glui::Button("Refresh Now", UIColors::Primary)) {
        refreshLeaderboard();
        leaderboardRefreshTimer = 0.0f;
    }
    
    if (glui::Button("Back", UIColors::Panel)) {
        setState(UIState::MAIN_MENU);
    }
}

void AccountUI::attemptLogin() {
    std::string username(usernameInput);
    std::string password(passwordInput);
    
    // Validate input
    if (username.empty() || password.empty()) {
        showMessage("Please enter username and password", UIColors::Error);
        return;
    }
    
    // First check credentials
    if (!accountManager->validateCredentials(username, password)) {
        showMessage("Invalid username or password", UIColors::Error);
        return;
    }
    
    // Check if account is already logged in (database-level session control)
    if (accountManager->isAccountLoggedIn(username)) {
        showMessage("Account already logged in from another location", UIColors::Error);
        return;
    }
    
    // Mark account as logged in (in database)
    if (!accountManager->setAccountLoggedIn(username, true)) {
        showMessage("Failed to establish session", UIColors::Error);
        return;
    }
    
    // Login successful
    isLoggedIn = true;
    currentUsername = username;
    sessionToken = "local_session"; // For single-player/local mode
    
    // Load account info
    currentAccount = accountManager->getAccount(username);
    
    showMessage("Login successful!", UIColors::Success);
    clearInputs();
    setState(UIState::MAIN_MENU);
}

void AccountUI::attemptRegister() {
    std::string username(usernameInput);
    std::string password(passwordInput);
    std::string confirmPassword(confirmPasswordInput);
    std::string email(emailInput);
    
    // Validation
    if (!validateUsername(username)) {
        showMessage("Username must be 3-20 characters", UIColors::Error);
        return;
    }
    
    if (!validatePassword(password)) {
        showMessage("Password must be at least 6 characters", UIColors::Error);
        return;
    }
    
    if (password != confirmPassword) {
        showMessage("Passwords do not match", UIColors::Error);
        return;
    }
    
    if (!validateEmail(email)) {
        showMessage("Invalid email address", UIColors::Error);
        return;
    }
    
    // Check if username exists
    if (accountManager->accountExists(username)) {
        showMessage("Username already exists", UIColors::Error);
        return;
    }
    
    // Check if email exists
    if (accountManager->emailExists(email)) {
        showMessage("Email already registered", UIColors::Error);
        return;
    }
    
    // Attempt registration
    if (accountManager->registerAccount(username, password, email)) {
        showMessage("Account created! Please login", UIColors::Success);
        clearInputs();
        setState(UIState::LOGIN_SCREEN);
    } else {
        showMessage("Registration failed. Please try again", UIColors::Error);
    }
}

void AccountUI::refreshAccountInfo() {
    if (isLoggedIn) {
        currentAccount = accountManager->getAccount(currentUsername);
        if (currentAccount) {
            showMessage("Account info refreshed", UIColors::Success);
        }
    }
}

void AccountUI::refreshLeaderboard() {
    leaderboardCache = accountManager->getTopPlayersForMode(leaderboardSelectedMode, 100);
}

void AccountUI::refreshMatchHistory() {
    if (isLoggedIn) {
        matchHistoryCache = accountManager->getMatchHistory(currentUsername, matchHistorySelectedMode, 20);
    }
}

void AccountUI::logout() {
    // Clear database login flag
    if (!currentUsername.empty()) {
        accountManager->setAccountLoggedIn(currentUsername, false);
    }
    
    isLoggedIn = false;
    currentUsername.clear();
    sessionToken.clear();
    currentAccount = nullptr;
    clearInputs();
    showMessage("Logged out successfully", UIColors::Success);
    setState(UIState::MAIN_MENU);
}

void AccountUI::setState(UIState newState) {
    currentState = newState;
    
    // Refresh account info when entering account info screen
    if (newState == UIState::ACCOUNT_INFO && isLoggedIn) {
        refreshAccountInfo();
    }
}

void AccountUI::showMessage(const std::string& message, const glm::vec4& color) {
    statusMessage = message;
    statusColor = color;
    messageTimer = 3.0f; // Show for 3 seconds
}

void AccountUI::clearInputs() {
    memset(usernameInput, 0, sizeof(usernameInput));
    memset(passwordInput, 0, sizeof(passwordInput));
    memset(emailInput, 0, sizeof(emailInput));
    memset(confirmPasswordInput, 0, sizeof(confirmPasswordInput));
}

bool AccountUI::validateUsername(const std::string& username) {
    if (username.length() < 3 || username.length() > 20) {
        return false;
    }
    
    // Check for valid characters (alphanumeric and underscore)
    for (char c : username) {
        if (!isalnum(c) && c != '_') {
            return false;
        }
    }
    
    return true;
}

bool AccountUI::validatePassword(const std::string& password) {
    return password.length() >= 6;
}

bool AccountUI::validateEmail(const std::string& email) {
    // Simple email validation
    size_t atPos = email.find('@');
    size_t dotPos = email.find_last_of('.');
    
    return (atPos != std::string::npos && 
            dotPos != std::string::npos && 
            dotPos > atPos && 
            email.length() > 5);
}

void AccountUI::setMatchSummary(const char* winnerName, int winnerKills, int totalPlayers, 
                                  const std::vector<PlayerScore>& scores, int gameMode) {
    strncpy(matchSummary.winnerName, winnerName, sizeof(matchSummary.winnerName) - 1);
    matchSummary.winnerName[sizeof(matchSummary.winnerName) - 1] = '\0';
    matchSummary.winnerKills = winnerKills;
    matchSummary.totalPlayers = totalPlayers;
    matchSummary.playerScores = scores;
    matchSummary.gameMode = gameMode;
}

void AccountUI::renderMatchSummary(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("===== MATCH SUMMARY =====", UIColors::Primary);
    glui::Space(20);
    
    // Display winner
    char winnerText[128];
    snprintf(winnerText, sizeof(winnerText), "Winner: %s", matchSummary.winnerName);
    glui::Text(winnerText, glm::vec4(1.0f, 0.84f, 0.0f, 1.0f));  // Gold color
    
    char killsText[64];
    snprintf(killsText, sizeof(killsText), "Kills: %d", matchSummary.winnerKills);
    glui::Text(killsText, UIColors::Success);
    
    glui::Space(20);
    glui::Text("===== FINAL SCOREBOARD =====", UIColors::White);
    glui::Space(10);
    
    // Display all player scores sorted by kills (descending)
    std::vector<PlayerScore> sortedScores = matchSummary.playerScores;
    std::sort(sortedScores.begin(), sortedScores.end(), 
              [](const PlayerScore& a, const PlayerScore& b) {
                  return a.kills > b.kills;  // Sort by kills descending
              });
    
    // Display each player's score
    for (size_t i = 0; i < sortedScores.size(); i++) {
        const auto& score = sortedScores[i];
        
        // Color based on rank
        glm::vec4 color = UIColors::White;
        if (i == 0) color = glm::vec4(1.0f, 0.84f, 0.0f, 1.0f);  // Gold
        else if (i == 1) color = glm::vec4(0.75f, 0.75f, 0.75f, 1.0f);  // Silver
        else if (i == 2) color = glm::vec4(0.8f, 0.5f, 0.2f, 1.0f);  // Bronze
        
        char scoreText[128];
        snprintf(scoreText, sizeof(scoreText), "%d. %s - Kills: %d | Deaths: %d | Score: %d",
                 (int)(i + 1), score.playerName, score.kills, score.deaths, score.score);
        glui::Text(scoreText, color);
        glui::Space(5);
    }
    
    glui::Space(20);
    
    // Button to return to browse rooms
    if (glui::Button("Return to Browse Rooms", UIColors::Primary)) {
        setState(UIState::BROWSE_ROOMS);
    }
    
    glui::Space(10);
    
    // Alternative: Return to main menu
    if (glui::Button("Return to Main Menu", UIColors::Panel)) {
        setState(UIState::MAIN_MENU);
    }
}

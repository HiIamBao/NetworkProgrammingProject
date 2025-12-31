#include "AccountUI.h"
#include "platformInput.h"
#include "imgui.h"
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

void AccountUI::render(gl2d::Renderer2D& renderer, gl2d::Font& font, float deltaTime) {
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
        default:
            break;
    }
}

void AccountUI::renderMainMenu(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("===== MULTIPLAYER GAME =====", UIColors::Primary);
    glui::Space(20);
    
    if (isLoggedIn) {
        // Show logged in user info
        std::string welcomeText = "Welcome, " + currentUsername + "!";
        glui::Text(welcomeText.c_str(), UIColors::Success);
        
        if (currentAccount) {
            char levelText[64];
            sprintf(levelText, "Level %d | Score: %d", currentAccount->level, currentAccount->totalScore);
            glui::Text(levelText, UIColors::White);
        }
        
        glui::Space(20);
        
        // Menu buttons for logged in user
        if (glui::Button("View Account Info", UIColors::Panel)) {
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
    glui::InputText("##username_login", usernameInput, sizeof(usernameInput));
    
    glui::Space(10);
    
    glui::Text("Password:", UIColors::White);
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
    glui::Text("===== ACCOUNT INFO =====", UIColors::Primary);
    glui::Space(20);
    
    if (currentAccount) {
        char buffer[256];
        
        sprintf(buffer, "Username: %s", currentAccount->username.c_str());
        glui::Text(buffer, UIColors::White);
        
        sprintf(buffer, "Email: %s", currentAccount->email.c_str());
        glui::Text(buffer, UIColors::White);
        
        glui::Space(10);
        
        sprintf(buffer, "Level: %d", currentAccount->level);
        glui::Text(buffer, UIColors::Success);
        
        sprintf(buffer, "Total Score: %d", currentAccount->totalScore);
        glui::Text(buffer, UIColors::Success);
        
        glui::Space(10);
        
        sprintf(buffer, "Games Played: %d", currentAccount->gamesPlayed);
        glui::Text(buffer, UIColors::White);
        
        sprintf(buffer, "Games Won: %d", currentAccount->gamesWon);
        glui::Text(buffer, UIColors::White);
        
        sprintf(buffer, "Win Rate: %.1f%%", currentAccount->winRate * 100.0f);
        glui::Text(buffer, UIColors::Warning);
        
        if (currentAccount->ranking > 0) {
            sprintf(buffer, "Ranking: %d", currentAccount->ranking);
            glui::Text(buffer, UIColors::Primary);
        }
        
        glui::Space(10);
        
        sprintf(buffer, "Member Since: %s", currentAccount->createdAt.c_str());
        glui::Text(buffer, UIColors::White);
    } else {
        glui::Text("Failed to load account info", UIColors::Error);
    }
    
    glui::Space(20);
    
    if (glui::Button("Refresh", UIColors::Primary)) {
        refreshAccountInfo();
    }
    
    if (glui::Button("Back", UIColors::Panel)) {
        setState(UIState::MAIN_MENU);
    }
}

void AccountUI::renderLeaderboard(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("===== LEADERBOARD =====", UIColors::Primary);
    glui::Text("(Refreshes every 5 seconds)", UIColors::White);
    glui::Space(20);
    
    if (leaderboardCache.empty()) {
        glui::Text("No players found", UIColors::White);
    } else {
        char buffer[256];
        int displayCount = std::min(20, (int)leaderboardCache.size());
        
        for (int i = 0; i < displayCount; i++) {
            const Account& acc = leaderboardCache[i];
            
            glm::vec4 color = UIColors::White;
            if (i == 0) color = glm::vec4(1.0f, 0.84f, 0.0f, 1.0f); // Gold
            else if (i == 1) color = glm::vec4(0.75f, 0.75f, 0.75f, 1.0f); // Silver
            else if (i == 2) color = glm::vec4(0.8f, 0.5f, 0.2f, 1.0f); // Bronze
            
            sprintf(buffer, "%d. %s - Lvl %d - Score: %d - W/L: %.1f%%", 
                    i + 1, acc.username.c_str(), acc.level, acc.totalScore, acc.winRate * 100.0f);
            glui::Text(buffer, color);
        }
    }
    
    glui::Space(20);
    
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
    
    // Attempt login (in single-player mode, just validate credentials)
    if (accountManager->validateCredentials(username, password)) {
        isLoggedIn = true;
        currentUsername = username;
        sessionToken = "local_session"; // For single-player/local mode
        
        // Load account info
        currentAccount = accountManager->getAccount(username);
        
        showMessage("Login successful!", UIColors::Success);
        clearInputs();
        setState(UIState::MAIN_MENU);
    } else {
        showMessage("Invalid username or password", UIColors::Error);
    }
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
    leaderboardCache = accountManager->getTopPlayers(100);
}

void AccountUI::logout() {
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

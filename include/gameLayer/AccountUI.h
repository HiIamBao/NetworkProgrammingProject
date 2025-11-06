#pragma once

#include <string>
#include <functional>
#include "gl2d/gl2d.h"
#include "glui/glui.h"
#include "AccountManager.h"
#include "SessionManager.h"

enum class UIState {
    MAIN_MENU,
    LOGIN_SCREEN,
    REGISTER_SCREEN,
    ACCOUNT_INFO,
    LEADERBOARD,
    BROWSE_ROOMS,
    HOST_SERVER,
    JOIN_SERVER,
    IN_GAME
};

namespace UIColors {
    inline const glm::vec4 Primary = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);
    inline const glm::vec4 Success = glm::vec4(0.2f, 0.8f, 0.3f, 1.0f);
    inline const glm::vec4 Error = glm::vec4(0.9f, 0.2f, 0.2f, 1.0f);
    inline const glm::vec4 Warning = glm::vec4(1.0f, 0.7f, 0.0f, 1.0f);
    inline const glm::vec4 Background = glm::vec4(0.1f, 0.1f, 0.15f, 0.95f);
    inline const glm::vec4 Panel = glm::vec4(0.15f, 0.15f, 0.2f, 0.95f);
    inline const glm::vec4 White = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
}

class AccountUI {
private:
    AccountManager* accountManager;
    SessionManager* sessionManager;
    
    // Current state
    UIState currentState;
    bool isLoggedIn;
    std::string currentUsername;
    std::string sessionToken;
    
    // Input buffers
    char usernameInput[32];
    char passwordInput[64];
    char emailInput[64];
    char confirmPasswordInput[64];
    
    // Message display
    std::string statusMessage;
    glm::vec4 statusColor;
    float messageTimer;
    
    // Account info cache
    Account* currentAccount;
    std::vector<Account> leaderboardCache;
    float leaderboardRefreshTimer;
    
public:
    AccountUI(AccountManager* accMgr, SessionManager* sessMgr);
    ~AccountUI();
    
    // Main render function
    void render(gl2d::Renderer2D& renderer, gl2d::Font& font, float deltaTime);
    
    // State management
    void setState(UIState newState);
    UIState getState() const { return currentState; }
    bool getIsLoggedIn() const { return isLoggedIn; }
    std::string getCurrentUsername() const { return currentUsername; }
    std::string getSessionToken() const { return sessionToken; }
    
    // Logout
    void logout();
    
    // Account info
    Account* getCurrentAccount() { return currentAccount; }
    
private:
    // Screen rendering
    void renderMainMenu(gl2d::Renderer2D& renderer, gl2d::Font& font);
    void renderLoginScreen(gl2d::Renderer2D& renderer, gl2d::Font& font);
    void renderRegisterScreen(gl2d::Renderer2D& renderer, gl2d::Font& font);
    void renderAccountInfo(gl2d::Renderer2D& renderer, gl2d::Font& font);
    void renderLeaderboard(gl2d::Renderer2D& renderer, gl2d::Font& font);
    
    // Actions
    void attemptLogin();
    void attemptRegister();
    void refreshAccountInfo();
    void refreshLeaderboard();
    
    // Utilities
    void showMessage(const std::string& message, const glm::vec4& color);
    void clearInputs();
    bool validateUsername(const std::string& username);
    bool validatePassword(const std::string& password);
    bool validateEmail(const std::string& email);
};

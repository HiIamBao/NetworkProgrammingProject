#pragma once
#include <gl2d/gl2d.h>
#include <gl2d/gl2d.h>
#include "NetworkClient.h"
#include "serverClient.h"
#include <AccountManager.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <common/Phisics.h>
#include "HordeDefense.h"
#include "BossFight.h"
#include "gameLayer.h" 
#include "GameRoom.h"
#include <common/tiles.h> 

// Forward declare Textures if possible or allow include from gameLayer.h
// Structure to hold chat messages
struct ChatMessage {
    std::string sender;
    std::string message;
    float time;
    glm::vec3 color;
};

class ClientGame {
public:
    ClientGame();
    ~ClientGame();

    // Main update loop called from gameLayer
    void update(float deltaTime, gl2d::Renderer2D& renderer, Textures& textures, const std::string& ip, const char* name, int port);
    void reset(int mapId = 0);
    void close(AccountManager& accountManager);

private:
   
    // Network handling
    NetworkClient network;
    void processNetworkPackets();
    
    // Game State
    GameMode currentGameMode = GameMode::DEATHMATCH;
    
    // Horde Defense Specific
    HordeDefense::HordeDefenseState hordeState = HordeDefense::HordeDefenseState::WAITING;
    int currentWave = 0;
    int totalWaves = 20;
    float phaseTimer = 0.0f;
    int playerMoney = 0;
    int enemiesAlive = 0;
    std::string waveNotification;
    float waveNotificationTimer = 0.0f;
    bool showShopUI = false;
    int selectedShopTab = 0;
    
    // Boss Fight Specific
    BossFight::Boss clientBoss;
    float bossNotificationTimer = 0.0f;
    glm::vec2 aoeAttackPos = {};
    float aoeAttackRadius = 0.0f;

    // Entities
    std::unordered_map<int32_t, phisics::Entity> players;
    std::unordered_map<int32_t, HordeDefense::Enemy> hordeEnemies;
    std::unordered_map<int32_t, BossFight::Minion> clientMinions;
    
    std::vector<phisics::Bullet> ownBullets;
    std::vector<phisics::Bullet> otherBullets;
    std::vector<phisics::Item> pickups;
    
    // Chat
    std::vector<ChatMessage> chatMessages;
    bool isChatActive = false;
    char chatInputBuffer[256] = {};

    // Camera
    gl2d::Camera camera;
    phisics::MapData map;
    
    // Map
    // Assuming tiles::Map or just Map? In client.cpp it was `gl2d::Renderer2D renderer; ... Map map;`
    // Let's check headers. tiles.h likely defines Map.
    // I need to include <common/tiles.h>
    
    // Helper constants
    const float maxChatTime = 10.0f;
    
    // Methods
    void updateShooting(float deltaTime, phisics::Entity& player, gl2d::Renderer2D& renderer);
    void updateCamera(float deltaTime, const phisics::Entity& player, gl2d::Renderer2D& renderer);
    void renderGame(gl2d::Renderer2D& renderer, Textures& textures, float deltaTime);
    void renderUI(gl2d::Renderer2D& renderer, Textures& textures);
};

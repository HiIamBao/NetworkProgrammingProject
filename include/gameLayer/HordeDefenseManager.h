#pragma once

#include "HordeDefense.h"
#include "packet.h"
#include <common/Phisics.h>
#include <enet/enet.h>
#include <vector>
#include <map>
#include <random>
#include <functional>

// ============================================================================
// HORDE DEFENSE MANAGER - Server-Side Game Logic
// ============================================================================
// Manages all Horde Defense game state, enemy spawning, wave progression,
// money system, shop validation, and AI behavior.
// ============================================================================

class HordeDefenseManager {
public:
    HordeDefenseManager();
    ~HordeDefenseManager();
    
    // ========================================================================
    // Initialization & Cleanup
    // ========================================================================
    
    void initialize();
    void cleanup();
    void reset();
    
    // ========================================================================
    // Main Update Loop
    // ========================================================================
    
    void update(float deltaTime);
    
    // ========================================================================
    // Game State Management
    // ========================================================================
    
    HordeDefense::HordeDefenseState getState() const { return currentState; }
    int getCurrentWave() const { return currentWave; }
    int getTotalWaves() const { return HordeDefense::TOTAL_WAVES; }
    float getPhaseTimer() const { return phaseTimer; }
    
    void setState(HordeDefense::HordeDefenseState newState);
    void startMatch();
    void endMatch(bool victory);
    
    // ========================================================================
    // Wave Management
    // ========================================================================
    
    void startWave();
    void completeWave();
    bool isWaveComplete() const;
    int getAliveEnemyCount() const;
    
    // ========================================================================
    // Enemy Management
    // ========================================================================
    
    void spawnEnemy(HordeDefense::EnemyType type, glm::vec2 position);
    void updateEnemies(float deltaTime, const std::map<int32_t, phisics::Entity>& players);
    void removeDeadEnemies();
    
    HordeDefense::Enemy* getEnemy(int32_t enemyId);
    const std::vector<HordeDefense::Enemy>& getEnemies() const { return enemies; }
    
    // Called when player shoots an enemy
    bool damageEnemy(int32_t enemyId, int damage, int32_t attackerCid, phisics::Entity* attacker = nullptr);
    
    // ========================================================================
    // Player Management
    // ========================================================================
    
    void addPlayer(int32_t cid);
    void removePlayer(int32_t cid);
    void respawnPlayer(int32_t cid, phisics::Entity& player);
    void markPlayerDead(int32_t cid);
    void markPlayerRespawned(int32_t cid);  // Mark player as respawned (prevents continuous respawn)
    bool isPlayerAlive(int32_t cid) const;
    void respawnAllDeadPlayers();  // Helper to respawn all dead players (called from server)
    bool needsRespawn(int32_t cid) const;  // Check if player needs respawn (dead but wave ended)
    
    // Decrement wave-based buffs when wave completes
    void decrementWaveBasedBuffs(std::map<int32_t, phisics::Entity>& players);
    
    int getAlivePlayers() const;
    bool allPlayersDead(const std::map<int32_t, phisics::Entity>& players) const;
    
    // ========================================================================
    // Money & Shop System
    // ========================================================================
    
    void awardMoney(int32_t cid, int amount, const char* reason);
    bool canAfford(int32_t cid, int cost) const;
    
    // Buy upgrade (returns success and response data)
    bool buyUpgrade(int32_t cid, phisics::Entity& player, HordeDefense::UpgradeType type, 
                    HordeBuyUpgradeResponse& response);
    
    // Buy item (returns success and response data)
    bool buyItem(int32_t cid, phisics::Entity& player, HordeDefense::ShopItemType type,
                 HordeBuyItemResponse& response);
    
    // Apply item effects to player
    void applyItemEffect(phisics::Entity& player, HordeDefense::ShopItemType type);
    
    // ========================================================================
    // Network Callbacks (set by server.cpp)
    // ========================================================================
    
    // Broadcast functions - set these before using
    using BroadcastFunc = std::function<void(Packet, const void*, size_t, bool)>;
    using SendToPlayerFunc = std::function<void(int32_t cid, Packet, const void*, size_t, bool)>;
    using PlayerDamageFunc = std::function<void(int32_t cid, int damage)>;  // cid, damage amount
    
    void setBroadcastCallback(BroadcastFunc callback) { broadcastCallback = callback; }
    void setSendToPlayerCallback(SendToPlayerFunc callback) { sendToPlayerCallback = callback; }
    void setPlayerDamageCallback(PlayerDamageFunc callback) { playerDamageCallback = callback; }
    
    // Send full game state to a newly joined player (for mid-game joins)
    void sendFullStateToPlayer(int32_t cid, ENetPeer* peer);
    
    // ========================================================================
    // Data Access
    // ========================================================================
    
    const std::map<int32_t, int>& getPlayerMoney() const { return playerMoney; }
    int getPlayerMoney(int32_t cid) const;
    
private:
    // ========================================================================
    // Internal State
    // ========================================================================
    
    HordeDefense::HordeDefenseState currentState;
    int currentWave;
    float phaseTimer;  // Countdown for current phase
    
    std::vector<HordeDefense::Enemy> enemies;
    int32_t nextEnemyId;
    
    std::map<int32_t, int> playerMoney;  // cid -> money
    std::map<int32_t, bool> playerAlive; // cid -> alive status
    std::map<int32_t, bool> playerRespawned; // cid -> respawned this wave (prevents multiple respawns)
    
    // Wave spawning
    HordeDefense::WaveConfig currentWaveConfig;
    float spawnTimer;
    int enemiesSpawnedThisWave;
    int totalEnemiesToSpawn;
    
    // Statistics
    int totalEnemiesKilled;
    std::map<int32_t, int> playerKills;  // cid -> kills this wave
    
    // Random number generation
    std::mt19937 rng;
    
    // Network callbacks
    BroadcastFunc broadcastCallback;
    SendToPlayerFunc sendToPlayerCallback;
    PlayerDamageFunc playerDamageCallback;
    
    // ========================================================================
    // Internal Helper Methods
    // ========================================================================
    
    void updateBuyPhase(float deltaTime);
    void updateWaveActive(float deltaTime);
    void updateEnemySpawning(float deltaTime);
    void updateEnemyAI(float deltaTime, const std::map<int32_t, phisics::Entity>& players);
    
    glm::vec2 getRandomSpawnPosition();
    int32_t findNearestPlayer(glm::vec2 enemyPos, const std::map<int32_t, phisics::Entity>& players);
    
    void broadcastStateUpdate();
    void broadcastEnemyUpdates();
    
    // Apply upgrade effects to player stats
    void applyUpgradeEffects(phisics::Entity& player) const;
    
    // Calculate effective stat with upgrades
    float getEffectiveDamageMultiplier(const phisics::Entity& player) const;
    float getEffectiveFireRateMultiplier(const phisics::Entity& player) const;
    float getEffectiveSpeedMultiplier(const phisics::Entity& player) const;
    float getEffectiveBulletSpeedMultiplier(const phisics::Entity& player) const;
    int getEffectiveMaxHealth(const phisics::Entity& player) const;
};

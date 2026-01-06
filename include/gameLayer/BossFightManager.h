#pragma once

#include "BossFight.h"
#include "packet.h"
#include <common/Phisics.h>
#include <enet/enet.h>
#include <vector>
#include <map>
#include <functional>
#include <random>

// ============================================================================
// BOSS FIGHT MANAGER - Server-Side Game Logic
// ============================================================================
// Manages all Boss Fight game state, boss AI, minion spawning, attack execution,
// and victory/defeat conditions.
// ============================================================================

class BossFightManager {
public:
    BossFightManager();
    ~BossFightManager();
    
    // ========================================================================
    // Initialization & Cleanup
    // ========================================================================
    
    void initialize();
    void cleanup();
    void reset();
    
    // ========================================================================
    // Main Update Loop
    // ========================================================================
    
    void update(float deltaTime, std::map<int32_t, phisics::Entity>& players, phisics::MapData* mapData = nullptr);
    
    // ========================================================================
    // Game State Management
    // ========================================================================
    
    BossFight::BossFightState getState() const { return currentState; }
    void setState(BossFight::BossFightState newState);
    void startMatch(phisics::MapData* mapData = nullptr);
    void endMatch(bool victory);
    float getMatchTime() const { return matchTime; }
    
    // ========================================================================
    // Boss Management
    // ========================================================================
    
    void spawnBoss(glm::vec2 position);
    void updateBoss(float deltaTime, const std::map<int32_t, phisics::Entity>& players);
    BossFight::Boss* getBoss() { return &boss; }
    const BossFight::Boss* getBoss() const { return &boss; }
    bool damageBoss(int damage, int32_t attackerCid);
    void updateBossPhase();
    
    // ========================================================================
    // Boss AI & Attacks
    // ========================================================================
    
    void selectBossTarget(const std::map<int32_t, phisics::Entity>& players);
    void executeBossAttack(std::map<int32_t, phisics::Entity>& players);
    void performMeleeAttack(std::map<int32_t, phisics::Entity>& players);
    void performAOESlam(std::map<int32_t, phisics::Entity>& players);
    void performCharge(std::map<int32_t, phisics::Entity>& players);
    void summonMinions();
    void updateProximityDamage(float deltaTime, std::map<int32_t, phisics::Entity>& players);
    
    // ========================================================================
    // Pathfinding & Movement
    // ========================================================================
    
    void moveBossWithPathfinding(float deltaTime, const std::map<int32_t, phisics::Entity>& players, phisics::MapData* mapData);
    std::vector<glm::vec2> findPath(glm::vec2 start, glm::vec2 end, phisics::MapData* mapData);
    bool isWalkable(int x, int y, phisics::MapData* mapData);
    void resolveBossCollision(phisics::MapData* mapData);
    void checkBossPlayerCollision(std::map<int32_t, phisics::Entity>& players, float deltaTime);
    
    // ========================================================================
    // Configuration (DEBUG)
    // ========================================================================
    
    void setProximityDamageRadius(float radius) { proximityDamageRadius = radius; }
    float getProximityDamageRadius() const { return proximityDamageRadius; }
    
    // ========================================================================
    // Minion Management
    // ========================================================================
    
    void spawnMinion(glm::vec2 position);
    void updateMinions(float deltaTime, std::map<int32_t, phisics::Entity>& players);
    void removeDeadMinions();
    bool damageMinion(int32_t minionId, int damage, int32_t attackerCid);
    const std::vector<BossFight::Minion>& getMinions() const { return minions; }
    
    // ========================================================================
    // Player Management
    // ========================================================================
    
    void addPlayer(int32_t cid);
    void removePlayer(int32_t cid);
    void respawnPlayer(int32_t cid, phisics::Entity& player);
    void markPlayerDead(int32_t cid);
    bool isPlayerAlive(int32_t cid) const;
    bool allPlayersDead() const;
    int getAlivePlayers() const;
    
    // ========================================================================
    // Network Callbacks (set by server.cpp)
    // ========================================================================
    
    using BroadcastFunc = std::function<void(Packet, const void*, size_t, bool)>;
    using SendToPlayerFunc = std::function<void(int32_t cid, Packet, const void*, size_t, bool)>;
    
    void setBroadcastCallback(BroadcastFunc callback) { broadcastCallback = callback; }
    void setSendToPlayerCallback(SendToPlayerFunc callback) { sendToPlayerCallback = callback; }
    
    // ========================================================================
    // Data Access
    // ========================================================================
    
    float getBossHealthPercent() const;
    
private:
    // ========================================================================
    // State Variables
    // ========================================================================
    
    BossFight::BossFightState currentState;
    BossFight::Boss boss;
    std::vector<BossFight::Minion> minions;
    int32_t nextMinionId;
    
    std::map<int32_t, bool> playerAlive;
    std::map<int32_t, int> playerDamageDealt;
    
    float matchTime;
    float spawnTimer;
    float bossUpdateTimer;      // For 10Hz boss updates
    float minionUpdateTimer;    // For 10Hz minion updates
    float proximityDamageTimer; // For proximity damage ticks
    float proximityDamageRadius; // Configurable damage zone radius
    int proximityDamageAmount;   // Damage per tick
    
    std::vector<glm::vec2> bossPath;  // Current pathfinding path
    int bossPathIndex;                 // Current waypoint in path
    float pathRecalcTimer;             // Time until next pathfinding update
    
    phisics::MapData* currentMap;      // Pointer to map for collision
    
    std::mt19937 rng;
    
    BroadcastFunc broadcastCallback;
    SendToPlayerFunc sendToPlayerCallback;
    
    // ========================================================================
    // Helper Methods
    // ========================================================================
    
    void broadcastStateUpdate();
    void broadcastBossUpdate();
    void broadcastMinionUpdates();
    glm::vec2 getRandomSpawnPosition();
    glm::vec2 getSafeRespawnPosition();
    glm::vec2 findValidBossSpawnPosition(phisics::MapData* mapData);
    int32_t findNearestPlayer(glm::vec2 position, const std::map<int32_t, phisics::Entity>& players);
    float distanceToPlayer(glm::vec2 pos1, glm::vec2 pos2);
    void moveBossTowardsTarget(float deltaTime, const std::map<int32_t, phisics::Entity>& players);
    void applyDamageToPlayer(int32_t cid, phisics::Entity& player, int damage, BossFight::BossAttackType attackType, glm::vec2 knockback);
};

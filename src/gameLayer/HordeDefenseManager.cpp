#include "HordeDefenseManager.h"
#include <iostream>
#include <cstring>
#include <algorithm>

using namespace HordeDefense;

// ============================================================================
// Constructor & Destructor
// ============================================================================

HordeDefenseManager::HordeDefenseManager() 
    : currentState(HordeDefenseState::WAITING)
    , currentWave(0)
    , phaseTimer(0.0f)
    , nextEnemyId(1)
    , spawnTimer(0.0f)
    , enemiesSpawnedThisWave(0)
    , totalEnemiesToSpawn(0)
    , totalEnemiesKilled(0)
{
    // Initialize random number generator
    std::random_device rd;
    rng.seed(rd());
}

HordeDefenseManager::~HordeDefenseManager() {
    cleanup();
}

// ============================================================================
// Initialization & Cleanup
// ============================================================================

void HordeDefenseManager::initialize() {
    reset();
    std::cout << "[HordeDefense] Manager initialized" << std::endl;
}

void HordeDefenseManager::cleanup() {
    enemies.clear();
    playerMoney.clear();
    playerAlive.clear();
    playerKills.clear();
}

void HordeDefenseManager::reset() {
    currentState = HordeDefenseState::WAITING;
    currentWave = 0;
    phaseTimer = 0.0f;
    nextEnemyId = 1;
    spawnTimer = 0.0f;
    enemiesSpawnedThisWave = 0;
    totalEnemiesToSpawn = 0;
    totalEnemiesKilled = 0;
    
    enemies.clear();
    playerKills.clear();
    
    std::cout << "[HordeDefense] Game state reset" << std::endl;
}

// ============================================================================
// Main Update Loop
// ============================================================================

void HordeDefenseManager::update(float deltaTime) {
    switch (currentState) {
        case HordeDefenseState::WAITING:
            // Waiting for match to start
            break;
            
        case HordeDefenseState::BUYING_PHASE:
            updateBuyPhase(deltaTime);
            break;
            
        case HordeDefenseState::WAVE_ACTIVE:
            updateWaveActive(deltaTime);
            break;
            
        case HordeDefenseState::WAVE_COMPLETE:
            // Transition handled explicitly
            break;
            
        case HordeDefenseState::VICTORY:
        case HordeDefenseState::DEFEAT:
            // Match ended
            break;
    }
}

// ============================================================================
// Game State Management
// ============================================================================

void HordeDefenseManager::setState(HordeDefenseState newState) {
    if (currentState == newState) return;
    
    std::cout << "[HordeDefense] State change: " << (int)currentState << " -> " << (int)newState << std::endl;
    currentState = newState;
    
    broadcastStateUpdate();
}

void HordeDefenseManager::startMatch() {
    reset();
    
    // Give all players starting money
    for (auto& [cid, money] : playerMoney) {
        money = STARTING_MONEY;
        
        // Send money update to player
        if (sendToPlayerCallback) {
            HordePlayerMoneyUpdate moneyData;
            moneyData.cid = cid;
            moneyData.newMoney = STARTING_MONEY;
            moneyData.changeAmount = STARTING_MONEY;
            strcpy(moneyData.reason, "Starting Money");
            
            Packet p;
            p.header = headerHordePlayerMoneyUpdate;
            p.cid = cid;
            sendToPlayerCallback(cid, p, &moneyData, sizeof(moneyData), true);
        }
    }
    
    // Start first wave (wave 1)
    currentWave = 1;
    setState(HordeDefenseState::BUYING_PHASE);
    phaseTimer = BUY_PHASE_DURATION;
    
    std::cout << "[HordeDefense] Match started! Wave 1 buy phase beginning..." << std::endl;
}

void HordeDefenseManager::endMatch(bool victory) {
    setState(victory ? HordeDefenseState::VICTORY : HordeDefenseState::DEFEAT);
    
    // Prepare match end data
    HordeMatchEndData endData;
    endData.victory = victory;
    endData.finalWave = currentWave;
    endData.totalKills = totalEnemiesKilled;
    endData.totalMoney = 0;
    
    // Find MVP (most kills)
    int32_t mvpCid = -1;
    int maxKills = 0;
    for (auto& [cid, kills] : playerKills) {
        if (kills > maxKills) {
            maxKills = kills;
            mvpCid = cid;
        }
    }
    
    endData.mvpPlayerId = mvpCid;
    endData.mvpKills = maxKills;
    strcpy(endData.mvpPlayerName, "Unknown"); // TODO: Get actual player name
    
    // Broadcast match end
    if (broadcastCallback) {
        Packet p;
        p.header = headerHordeMatchEnd;
        p.cid = 0;
        broadcastCallback(p, &endData, sizeof(endData), true);
    }
    
    std::cout << "[HordeDefense] Match ended! Victory: " << victory << ", Final wave: " << currentWave << std::endl;
}

// ============================================================================
// Wave Management
// ============================================================================

void HordeDefenseManager::startWave() {
    setState(HordeDefenseState::WAVE_ACTIVE);
    
    // Get wave configuration
    currentWaveConfig = WaveConfig::getWaveConfig(currentWave);
    totalEnemiesToSpawn = currentWaveConfig.getTotalEnemies();
    enemiesSpawnedThisWave = 0;
    spawnTimer = 0.0f;
    
    // Clear wave kills
    playerKills.clear();
    
    // Broadcast wave start
    if (broadcastCallback) {
        HordeWaveStartData waveData;
        waveData.waveNumber = currentWave;
        waveData.totalEnemies = currentWaveConfig.getTotalEnemies();
        waveData.zombieCount = currentWaveConfig.zombieCount;
        waveData.runnerCount = currentWaveConfig.runnerCount;
        waveData.tankCount = currentWaveConfig.tankCount;
        waveData.exploderCount = currentWaveConfig.exploderCount;
        waveData.bossCount = currentWaveConfig.bossCount;
        
        Packet p;
        p.header = headerHordeWaveStart;
        p.cid = 0;
        broadcastCallback(p, &waveData, sizeof(waveData), true);
    }
    
    std::cout << "[HordeDefense] Wave " << currentWave << " started! Enemies: " << totalEnemiesToSpawn << std::endl;
}

void HordeDefenseManager::completeWave() {
    setState(HordeDefenseState::WAVE_COMPLETE);
    
    // Mark all dead players for respawn (actual respawn happens in server.cpp)
    respawnAllDeadPlayers();
    
    // Award completion bonus
    int bonus = currentWaveConfig.completionBonus;
    for (auto& [cid, money] : playerMoney) {
        awardMoney(cid, bonus, "Wave Bonus");
    }
    
    // Find MVP for this wave
    int32_t mvpCid = -1;
    int maxKills = 0;
    for (auto& [cid, kills] : playerKills) {
        if (kills > maxKills) {
            maxKills = kills;
            mvpCid = cid;
        }
    }
    
    // Broadcast wave complete
    if (broadcastCallback) {
        HordeWaveCompleteData completeData;
        completeData.waveNumber = currentWave;
        completeData.completionBonus = bonus;
        completeData.totalKills = 0;
        for (auto& [cid, kills] : playerKills) {
            completeData.totalKills += kills;
        }
        completeData.mvpPlayerId = mvpCid;
        
        Packet p;
        p.header = headerHordeWaveComplete;
        p.cid = 0;
        broadcastCallback(p, &completeData, sizeof(completeData), true);
    }
    
    std::cout << "[HordeDefense] Wave " << currentWave << " complete! Bonus: $" << bonus << std::endl;
    
    // Check for victory
    if (currentWave >= TOTAL_WAVES) {
        endMatch(true);  // Victory!
        return;
    }
    
    // Move to next wave
    currentWave++;
    setState(HordeDefenseState::BUYING_PHASE);
    phaseTimer = BUY_PHASE_DURATION;
}

bool HordeDefenseManager::isWaveComplete() const {
    // Wave is complete when all enemies are spawned and all are dead
    return enemiesSpawnedThisWave >= totalEnemiesToSpawn && getAliveEnemyCount() == 0;
}

int HordeDefenseManager::getAliveEnemyCount() const {
    int count = 0;
    for (const auto& enemy : enemies) {
        if (enemy.isAlive) count++;
    }
    return count;
}

// ============================================================================
// Enemy Management
// ============================================================================

void HordeDefenseManager::spawnEnemy(EnemyType type, glm::vec2 position) {
    Enemy enemy;
    enemy.id = nextEnemyId++;
    enemy.type = type;
    enemy.position = position;
    enemy.velocity = glm::vec2(0, 0);
    enemy.targetPlayerId = -1;
    enemy.isAlive = true;
    enemy.lastAttackTime = 0.0f;
    
    // Get base stats
    EnemyStats stats = EnemyStats::getStats(type);
    enemy.maxHealth = stats.baseHealth;
    enemy.health = stats.baseHealth;
    enemy.speed = stats.baseSpeed;
    enemy.damage = stats.baseDamage;
    
    enemies.push_back(enemy);
    
    // Broadcast spawn
    if (broadcastCallback) {
        HordeEnemySpawnData spawnData;
        spawnData.enemyId = enemy.id;
        spawnData.enemyType = (int)type;
        spawnData.posX = position.x;
        spawnData.posY = position.y;
        spawnData.health = enemy.health;
        spawnData.maxHealth = enemy.maxHealth;
        
        Packet p;
        p.header = headerHordeSpawnEnemy;
        p.cid = 0;
        broadcastCallback(p, &spawnData, sizeof(spawnData), true);
    }
}

void HordeDefenseManager::updateEnemies(float deltaTime, const std::map<int32_t, phisics::Entity>& players) {
    updateEnemyAI(deltaTime, players);
}

void HordeDefenseManager::removeDeadEnemies() {
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(),
            [](const Enemy& e) { return !e.isAlive; }),
        enemies.end()
    );
}

Enemy* HordeDefenseManager::getEnemy(int32_t enemyId) {
    for (auto& enemy : enemies) {
        if (enemy.id == enemyId && enemy.isAlive) {
            return &enemy;
        }
    }
    return nullptr;
}

bool HordeDefenseManager::damageEnemy(int32_t enemyId, int damage, int32_t attackerCid) {
    Enemy* enemy = getEnemy(enemyId);
    if (!enemy) return false;
    
    enemy->health -= damage;
    
    if (enemy->health <= 0) {
        enemy->isAlive = false;
        enemy->health = 0;
        
        // Award money
        EnemyStats stats = EnemyStats::getStats(enemy->type);
        awardMoney(attackerCid, stats.moneyReward, "Enemy Kill");
        
        // Track kills
        playerKills[attackerCid]++;
        totalEnemiesKilled++;
        
        // Broadcast death
        if (broadcastCallback) {
            HordeEnemyDeathData deathData;
            deathData.enemyId = enemyId;
            deathData.killerCid = attackerCid;
            deathData.moneyReward = stats.moneyReward;
            deathData.enemyType = (int)enemy->type;
            deathData.posX = enemy->position.x;
            deathData.posY = enemy->position.y;
            
            Packet p;
            p.header = headerHordeEnemyDeath;
            p.cid = 0;
            broadcastCallback(p, &deathData, sizeof(deathData), true);
        }
        
        return true;  // Enemy killed
    }
    
    return false;  // Enemy damaged but alive
}

// ============================================================================
// Player Management
// ============================================================================

void HordeDefenseManager::addPlayer(int32_t cid) {
    playerMoney[cid] = 0;
    playerAlive[cid] = true;
    playerRespawned[cid] = false;
    playerKills[cid] = 0;
    
    std::cout << "[HordeDefense] Player " << cid << " added" << std::endl;
}

void HordeDefenseManager::removePlayer(int32_t cid) {
    playerMoney.erase(cid);
    playerAlive.erase(cid);
    playerRespawned.erase(cid);
    playerKills.erase(cid);
    
    std::cout << "[HordeDefense] Player " << cid << " removed" << std::endl;
}

void HordeDefenseManager::respawnPlayer(int32_t cid, phisics::Entity& player) {
    std::cout << "[HordeDefense] respawnPlayer() called for CID " << cid << ", current HP: " << player.life << std::endl;
    
    playerAlive[cid] = true;
    playerRespawned[cid] = true;  // Mark as respawned to prevent multiple respawns
    
    int newHP = getEffectiveMaxHealth(player);
    std::cout << "[HordeDefense] Calculated max HP for player " << cid << ": " << newHP << std::endl;
    player.life = newHP;  // Respawn with full HP (including upgrades)
    
    // Reset temporary buffs (wave-based)
    player.speedBoostWaves = 0;
    player.damageBoostWaves = 0;
    player.multiShotWaves = 0;
    player.shieldHealth = 0.0f;
    
    glm::vec2 spawnPos = getRandomSpawnPosition();
    player.pos = spawnPos;
    
    std::cout << "[HordeDefense] Player " << cid << " respawned with HP: " << player.life << " at position (" << spawnPos.x << ", " << spawnPos.y << ")" << std::endl;
    
    // Broadcast respawn
    if (broadcastCallback) {
        HordePlayerRespawnData respawnData;
        respawnData.cid = cid;
        respawnData.posX = spawnPos.x;
        respawnData.posY = spawnPos.y;
        
        Packet p;
        p.header = headerHordePlayerRespawn;
        p.cid = cid;
        broadcastCallback(p, &respawnData, sizeof(respawnData), true);
    }
}

void HordeDefenseManager::markPlayerDead(int32_t cid) {
    playerAlive[cid] = false;
    playerRespawned[cid] = false;  // Reset respawn flag when player dies
    std::cout << "[HordeDefense] Player " << cid << " marked as dead" << std::endl;
}

void HordeDefenseManager::markPlayerRespawned(int32_t cid) {
    playerRespawned[cid] = true;  // Set flag to prevent multiple respawns
    std::cout << "[HordeDefense] Player " << cid << " marked as respawned" << std::endl;
}

bool HordeDefenseManager::isPlayerAlive(int32_t cid) const {
    auto it = playerAlive.find(cid);
    return (it != playerAlive.end() && it->second);
}

bool HordeDefenseManager::needsRespawn(int32_t cid) const {
    // Player needs respawn if they're marked as alive but haven't been respawned yet
    auto aliveIt = playerAlive.find(cid);
    auto respawnedIt = playerRespawned.find(cid);
    
    bool isAlive = (aliveIt != playerAlive.end() && aliveIt->second);
    bool hasBeenRespawned = (respawnedIt != playerRespawned.end() && respawnedIt->second);
    
    // Needs respawn if marked alive but not yet respawned
    return isAlive && !hasBeenRespawned;
}

void HordeDefenseManager::respawnAllDeadPlayers() {
    // Just mark players for respawn - actual respawn happens in server loop
    // This is called during wave complete transition
}

void HordeDefenseManager::decrementWaveBasedBuffs(std::map<int32_t, phisics::Entity>& players) {
    for (auto& [cid, player] : players) {
        bool changed = false;
        
        if (player.speedBoostWaves > 0) {
            player.speedBoostWaves--;
            changed = true;
            if (player.speedBoostWaves == 0) {
                std::cout << "[HordeDefense] Speed boost expired for player " << cid << std::endl;
            }
        }
        
        if (player.damageBoostWaves > 0) {
            player.damageBoostWaves--;
            changed = true;
            if (player.damageBoostWaves == 0) {
                std::cout << "[HordeDefense] Damage boost expired for player " << cid << std::endl;
            }
        }
        
        if (player.multiShotWaves > 0) {
            player.multiShotWaves--;
            changed = true;
            if (player.multiShotWaves == 0) {
                std::cout << "[HordeDefense] Multi-shot expired for player " << cid << std::endl;
            }
        }
        
        // If buffs changed, broadcast update
        if (changed && broadcastCallback) {
            HordePlayerStatsUpdate statsUpdate;
            statsUpdate.cid = cid;
            statsUpdate.damageLevel = player.damageUpgradeLevel;
            statsUpdate.fireRateLevel = player.fireRateUpgradeLevel;
            statsUpdate.healthLevel = player.healthUpgradeLevel;
            statsUpdate.speedLevel = player.speedUpgradeLevel;
            statsUpdate.bulletSpeedLevel = player.bulletSpeedUpgradeLevel;
            statsUpdate.speedBoostWaves = player.speedBoostWaves;
            statsUpdate.damageBoostWaves = player.damageBoostWaves;
            statsUpdate.multiShotWaves = player.multiShotWaves;
            statsUpdate.shieldHealth = player.shieldHealth;
            
            Packet p;
            p.header = headerHordePlayerStatsUpdate;
            p.cid = 0;
            broadcastCallback(p, &statsUpdate, sizeof(statsUpdate), true);
        }
    }
}

// ============================================================================
// Money & Shop System
// ============================================================================

void HordeDefenseManager::awardMoney(int32_t cid, int amount, const char* reason) {
    playerMoney[cid] += amount;
    
    // Send money update
    if (sendToPlayerCallback) {
        HordePlayerMoneyUpdate moneyData;
        moneyData.cid = cid;
        moneyData.newMoney = playerMoney[cid];
        moneyData.changeAmount = amount;
        strncpy(moneyData.reason, reason, sizeof(moneyData.reason) - 1);
        moneyData.reason[sizeof(moneyData.reason) - 1] = '\0';
        
        Packet p;
        p.header = headerHordePlayerMoneyUpdate;
        p.cid = cid;
        sendToPlayerCallback(cid, p, &moneyData, sizeof(moneyData), true);
    }
}

bool HordeDefenseManager::canAfford(int32_t cid, int cost) const {
    auto it = playerMoney.find(cid);
    if (it == playerMoney.end()) return false;
    return it->second >= cost;
}

bool HordeDefenseManager::buyUpgrade(int32_t cid, phisics::Entity& player, UpgradeType type, HordeBuyUpgradeResponse& response) {
    UpgradeInfo info = UpgradeInfo::getInfo(type);
    
    // Get current level
    int currentLevel = 0;
    switch (type) {
        case UpgradeType::DAMAGE: currentLevel = player.damageUpgradeLevel; break;
        case UpgradeType::FIRE_RATE: currentLevel = player.fireRateUpgradeLevel; break;
        case UpgradeType::HEALTH: currentLevel = player.healthUpgradeLevel; break;
        case UpgradeType::SPEED: currentLevel = player.speedUpgradeLevel; break;
        case UpgradeType::BULLET_SPEED: currentLevel = player.bulletSpeedUpgradeLevel; break;
    }
    
    // Check if already max level
    if (currentLevel >= info.maxLevel) {
        response.success = false;
        response.upgradeType = (int)type;
        response.newLevel = currentLevel;
        response.newMoney = playerMoney[cid];
        strcpy(response.message, "Already at max level");
        return false;
    }
    
    // Calculate cost for next level
    int nextLevel = currentLevel + 1;
    int cost = info.getCostForLevel(nextLevel);
    
    // Check if can afford
    if (!canAfford(cid, cost)) {
        response.success = false;
        response.upgradeType = (int)type;
        response.newLevel = currentLevel;
        response.newMoney = playerMoney[cid];
        strcpy(response.message, "Not enough money");
        return false;
    }
    
    // Deduct money
    playerMoney[cid] -= cost;
    
    // Apply upgrade
    switch (type) {
        case UpgradeType::DAMAGE: 
            player.damageUpgradeLevel = nextLevel; 
            break;
        case UpgradeType::FIRE_RATE: 
            player.fireRateUpgradeLevel = nextLevel; 
            break;
        case UpgradeType::HEALTH: {
            player.healthUpgradeLevel = nextLevel;
            // Update max health
            int oldMaxLife = player.maxLife;
            player.maxLife = getEffectiveMaxHealth(player);
            // Heal player by the amount maxLife increased
            int healthIncrease = player.maxLife - oldMaxLife;
            player.life = std::min(player.life + healthIncrease, player.maxLife);
            std::cout << "[HordeDefense] Health upgrade: MaxHP " << oldMaxLife << " -> " << player.maxLife 
                      << ", CurrentHP: " << player.life << std::endl;
            break;
        }
        case UpgradeType::SPEED: 
            player.speedUpgradeLevel = nextLevel; 
            break;
        case UpgradeType::BULLET_SPEED: 
            player.bulletSpeedUpgradeLevel = nextLevel; 
            break;
    }
    
    // Success response
    response.success = true;
    response.upgradeType = (int)type;
    response.newLevel = nextLevel;
    response.newMoney = playerMoney[cid];
    strcpy(response.message, "Upgrade purchased!");
    
    std::cout << "[HordeDefense] Player " << cid << " upgraded " << info.name << " to level " << nextLevel << std::endl;
    
    return true;
}

bool HordeDefenseManager::buyItem(int32_t cid, phisics::Entity& player, ShopItemType type, HordeBuyItemResponse& response) {
    ShopItemInfo info = ShopItemInfo::getInfo(type);
    
    // Check if can afford
    if (!canAfford(cid, info.cost)) {
        response.success = false;
        response.itemType = (int)type;
        response.newMoney = playerMoney[cid];
        response.effectValue = 0;
        response.duration = 0;
        strcpy(response.message, "Not enough money");
        return false;
    }
    
    // Deduct money
    playerMoney[cid] -= info.cost;
    
    // Apply item effect
    applyItemEffect(player, type);
    
    // Success response
    response.success = true;
    response.itemType = (int)type;
    response.newMoney = playerMoney[cid];
    response.effectValue = info.effectValue;
    response.duration = info.duration;
    strcpy(response.message, "Item purchased!");
    
    std::cout << "[HordeDefense] Player " << cid << " bought " << info.name << std::endl;
    
    return true;
}

void HordeDefenseManager::applyItemEffect(phisics::Entity& player, ShopItemType type) {
    ShopItemInfo info = ShopItemInfo::getInfo(type);
    
    switch (type) {
        case ShopItemType::HEALTH_PACK:
            player.life = std::min(player.life + (int)info.effectValue, player.maxLife);
            std::cout << "[HordeDefense] Health pack used: player.life=" << player.life << "/" << player.maxLife << std::endl;
            break;
            
        case ShopItemType::SPEED_BOOST:
            // Add waves (not replace) so multiple purchases stack the duration
            player.speedBoostWaves += (int)info.duration;
            std::cout << "[HordeDefense] Speed boost applied: " << player.speedBoostWaves << " waves remaining" << std::endl;
            break;
            
        case ShopItemType::DAMAGE_AMPLIFIER:
            // Add waves (not replace) so multiple purchases stack the duration
            player.damageBoostWaves += (int)info.duration;
            std::cout << "[HordeDefense] Damage amplifier applied: " << player.damageBoostWaves << " waves remaining" << std::endl;
            break;
            
        case ShopItemType::MULTI_SHOT:
            // Add waves (not replace) so multiple purchases stack the duration
            player.multiShotWaves += (int)info.duration;
            std::cout << "[HordeDefense] Multi-shot applied: " << player.multiShotWaves << " waves remaining" << std::endl;
            break;
    }
}

// ============================================================================
// Data Access
// ============================================================================

int HordeDefenseManager::getPlayerMoney(int32_t cid) const {
    auto it = playerMoney.find(cid);
    if (it != playerMoney.end()) {
        return it->second;
    }
    return 0;
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

void HordeDefenseManager::updateBuyPhase(float deltaTime) {
    phaseTimer -= deltaTime;
    
    if (phaseTimer <= 0) {
        // Buy phase over, start wave
        startWave();
    }
    
    // Broadcast state update every second
    static float broadcastTimer = 0;
    broadcastTimer += deltaTime;
    if (broadcastTimer >= 1.0f) {
        broadcastTimer = 0;
        broadcastStateUpdate();
    }
}

void HordeDefenseManager::updateWaveActive(float deltaTime) {
    // Spawn enemies
    updateEnemySpawning(deltaTime);
    
    // Check if wave complete
    if (isWaveComplete()) {
        completeWave();
    }
    
    // Broadcast enemy updates (10Hz = every 0.1s)
    static float enemyBroadcastTimer = 0;
    enemyBroadcastTimer += deltaTime;
    if (enemyBroadcastTimer >= 0.1f) {
        enemyBroadcastTimer = 0;
        broadcastEnemyUpdates();
    }
}

void HordeDefenseManager::updateEnemySpawning(float deltaTime) {
    if (enemiesSpawnedThisWave >= totalEnemiesToSpawn) {
        return;  // All enemies spawned
    }
    
    spawnTimer += deltaTime;
    
    if (spawnTimer >= currentWaveConfig.spawnInterval) {
        spawnTimer = 0;
        
        // Determine which type to spawn based on wave config
        // Spawn in order: zombies, runners, tanks, exploders, bosses
        EnemyType typeToSpawn;
        
        int zombiesSpawned = 0, runnersSpawned = 0, tanksSpawned = 0, explodersSpawned = 0, bossesSpawned = 0;
        
        for (const auto& enemy : enemies) {
            switch (enemy.type) {
                case EnemyType::ZOMBIE: zombiesSpawned++; break;
                case EnemyType::RUNNER: runnersSpawned++; break;
                case EnemyType::TANK: tanksSpawned++; break;
                case EnemyType::EXPLODER: explodersSpawned++; break;
                case EnemyType::BOSS: bossesSpawned++; break;
            }
        }
        
        if (zombiesSpawned < currentWaveConfig.zombieCount) {
            typeToSpawn = EnemyType::ZOMBIE;
        } else if (runnersSpawned < currentWaveConfig.runnerCount) {
            typeToSpawn = EnemyType::RUNNER;
        } else if (tanksSpawned < currentWaveConfig.tankCount) {
            typeToSpawn = EnemyType::TANK;
        } else if (explodersSpawned < currentWaveConfig.exploderCount) {
            typeToSpawn = EnemyType::EXPLODER;
        } else {
            typeToSpawn = EnemyType::BOSS;
        }
        
        glm::vec2 spawnPos = getRandomSpawnPosition();
        spawnEnemy(typeToSpawn, spawnPos);
        enemiesSpawnedThisWave++;
    }
}

void HordeDefenseManager::updateEnemyAI(float deltaTime, const std::map<int32_t, phisics::Entity>& players) {
    const float ATTACK_RANGE = 1.0f;  // Distance at which enemy can attack
    const float ATTACK_COOLDOWN = 1.0f;  // Seconds between attacks
    
    for (auto& enemy : enemies) {
        if (!enemy.isAlive) continue;
        
        // Update attack cooldown
        if (enemy.lastAttackTime > 0) {
            enemy.lastAttackTime -= deltaTime;
        }
        
        // Find nearest ALIVE player
        enemy.targetPlayerId = findNearestPlayer(enemy.position, players);
        
        if (enemy.targetPlayerId != -1) {
            auto it = players.find(enemy.targetPlayerId);
            if (it != players.end()) {
                const phisics::Entity& target = it->second;
                
                // Skip if target is dead
                if (target.life <= 0) {
                    enemy.targetPlayerId = -1;
                    enemy.velocity = glm::vec2(0, 0);
                    continue;
                }
                
                // Calculate distance to target
                glm::vec2 direction = target.pos - enemy.position;
                float distance = glm::length(direction);
                
                // Check if within attack range
                if (distance <= ATTACK_RANGE) {
                    // Stop moving
                    enemy.velocity = glm::vec2(0, 0);
                    
                    // Attack if cooldown is ready
                    if (enemy.lastAttackTime <= 0) {
                        enemy.lastAttackTime = ATTACK_COOLDOWN;
                        
                        // Check if target player is alive
                        auto playerIt = players.find(enemy.targetPlayerId);
                        if (playerIt != players.end() && playerIt->second.life > 0) {
                            const phisics::Entity& targetPlayer = playerIt->second;
                            
                            int remainingDamage = enemy.damage;
                            
                            // Calculate actual damage (shield absorbs first)
                            if (targetPlayer.shieldHealth > 0) {
                                int shieldDamage = std::min((int)targetPlayer.shieldHealth, remainingDamage);
                                remainingDamage -= shieldDamage;
                            }
                            
                            // Notify server to apply damage via callback
                            if (remainingDamage > 0 && playerDamageCallback) {
                                playerDamageCallback(enemy.targetPlayerId, remainingDamage);
                            }
                        }
                        
                        // Broadcast attack to clients (for visual/audio feedback)
                        if (broadcastCallback) {
                            HordeEnemyAttackData attackData;
                            attackData.enemyId = enemy.id;
                            attackData.targetCid = enemy.targetPlayerId;
                            attackData.damage = enemy.damage;
                            attackData.enemyType = (int)enemy.type;
                            
                            Packet p;
                            p.header = headerHordeEnemyAttack;
                            p.cid = 0;
                            broadcastCallback(p, &attackData, sizeof(attackData), true);
                        }
                    }
                } else if (distance > 0.01f) {
                    // Move towards player
                    direction = glm::normalize(direction);
                    enemy.velocity = direction * enemy.speed;
                    enemy.position += enemy.velocity * deltaTime;
                }
            }
        }
    }
}

glm::vec2 HordeDefenseManager::getRandomSpawnPosition() {
    // TODO: Use actual map spawn points
    // For now, spawn in random positions around the map edges
    std::uniform_real_distribution<float> dist(5.0f, 20.0f);
    std::uniform_int_distribution<int> sideDist(0, 3);
    
    int side = sideDist(rng);
    glm::vec2 pos;
    
    switch (side) {
        case 0: pos = glm::vec2(dist(rng), 0); break;      // Top
        case 1: pos = glm::vec2(20, dist(rng)); break;     // Right
        case 2: pos = glm::vec2(dist(rng), 20); break;     // Bottom
        case 3: pos = glm::vec2(0, dist(rng)); break;      // Left
    }
    
    return pos;
}

int32_t HordeDefenseManager::findNearestPlayer(glm::vec2 enemyPos, const std::map<int32_t, phisics::Entity>& players) {
    int32_t nearestCid = -1;
    float nearestDist = FLT_MAX;
    
    for (const auto& [cid, player] : players) {
        if (player.life <= 0) continue;  // Skip dead players
        
        float dist = glm::distance(enemyPos, player.pos);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearestCid = cid;
        }
    }
    
    return nearestCid;
}

void HordeDefenseManager::broadcastStateUpdate() {
    if (!broadcastCallback) return;
    
    HordeStateUpdateData stateData;
    stateData.currentWave = currentWave;
    stateData.gameState = (int)currentState;
    stateData.timeRemaining = phaseTimer;
    stateData.playersAlive = getAlivePlayers();
    stateData.enemiesRemaining = getAliveEnemyCount();
    
    Packet p;
    p.header = headerHordeStateUpdate;
    p.cid = 0;
    broadcastCallback(p, &stateData, sizeof(stateData), true);
}

void HordeDefenseManager::broadcastEnemyUpdates() {
    if (!broadcastCallback || enemies.empty()) return;
    
    // Batch enemy updates
    std::vector<HordeEnemyUpdateData> updates;
    for (const auto& enemy : enemies) {
        if (!enemy.isAlive) continue;
        
        HordeEnemyUpdateData update;
        update.enemyId = enemy.id;
        update.posX = enemy.position.x;
        update.posY = enemy.position.y;
        update.health = enemy.health;
        update.targetPlayerId = enemy.targetPlayerId;
        
        updates.push_back(update);
    }
    
    if (!updates.empty()) {
        Packet p;
        p.header = headerHordeEnemyUpdate;
        p.cid = 0;
        size_t dataSize = sizeof(HordeEnemyUpdateData) * updates.size();
        broadcastCallback(p, updates.data(), dataSize, false);  // Unreliable
    }
}

// ============================================================================
// Stat Calculation Helpers
// ============================================================================

float HordeDefenseManager::getEffectiveDamageMultiplier(const phisics::Entity& player) const {
    UpgradeInfo info = UpgradeInfo::getInfo(UpgradeType::DAMAGE);
    float multiplier = 1.0f + (player.damageUpgradeLevel * info.effectPerLevel);
    
    // Add damage boost buff (wave-based)
    if (player.damageBoostWaves > 0) {
        multiplier += 1.0f;  // +100% damage
    }
    
    return multiplier;
}

float HordeDefenseManager::getEffectiveFireRateMultiplier(const phisics::Entity& player) const {
    UpgradeInfo info = UpgradeInfo::getInfo(UpgradeType::FIRE_RATE);
    return 1.0f + (player.fireRateUpgradeLevel * info.effectPerLevel);
}

float HordeDefenseManager::getEffectiveSpeedMultiplier(const phisics::Entity& player) const {
    UpgradeInfo info = UpgradeInfo::getInfo(UpgradeType::SPEED);
    float multiplier = 1.0f + (player.speedUpgradeLevel * info.effectPerLevel);
    
    // Add speed boost buff (wave-based)
    if (player.speedBoostWaves > 0) {
        multiplier += 0.5f;  // +50% speed
    }
    
    return multiplier;
}

float HordeDefenseManager::getEffectiveBulletSpeedMultiplier(const phisics::Entity& player) const {
    UpgradeInfo info = UpgradeInfo::getInfo(UpgradeType::BULLET_SPEED);
    return 1.0f + (player.bulletSpeedUpgradeLevel * info.effectPerLevel);
}

int HordeDefenseManager::getEffectiveMaxHealth(const phisics::Entity& player) const {
    UpgradeInfo info = UpgradeInfo::getInfo(UpgradeType::HEALTH);
    int maxHP = player.maxLife + (int)(player.healthUpgradeLevel * info.effectPerLevel);
    return maxHP;
}

void HordeDefenseManager::sendFullStateToPlayer(int32_t cid, ENetPeer* peer) {
    if (!sendToPlayerCallback) return;
    
    std::cout << "[HordeDefense] Sending full state to newly joined player " << cid << std::endl;
    
    // Send current game state
    HordeStateUpdateData stateData;
    stateData.currentWave = currentWave;
    stateData.gameState = (int)currentState;
    stateData.timeRemaining = phaseTimer;
    stateData.playersAlive = getAlivePlayers();
    stateData.enemiesRemaining = getAliveEnemyCount();
    
    Packet p;
    p.header = headerHordeStateUpdate;
    p.cid = 0;
    sendToPlayerCallback(cid, p, &stateData, sizeof(stateData), true);
    
    // Send all existing enemies (spawn packets)
    for (const auto& enemy : enemies) {
        if (!enemy.isAlive) continue;
        
        HordeEnemySpawnData spawnData;
        spawnData.enemyId = enemy.id;
        spawnData.enemyType = (int)enemy.type;
        spawnData.posX = enemy.position.x;
        spawnData.posY = enemy.position.y;
        spawnData.health = enemy.health;
        spawnData.maxHealth = enemy.maxHealth;
        
        Packet sp;
        sp.header = headerHordeSpawnEnemy;
        sp.cid = 0;
        sendToPlayerCallback(cid, sp, &spawnData, sizeof(spawnData), true);
    }
    
    // Send player's money
    auto moneyIt = playerMoney.find(cid);
    if (moneyIt != playerMoney.end()) {
        HordePlayerMoneyUpdate moneyData;
        moneyData.cid = cid;
        moneyData.newMoney = moneyIt->second;
        strncpy(moneyData.reason, "Starting Money", sizeof(moneyData.reason) - 1);
        
        Packet mp;
        mp.header = headerHordePlayerMoneyUpdate;
        mp.cid = 0;
        sendToPlayerCallback(cid, mp, &moneyData, sizeof(moneyData), true);
    }
    
    std::cout << "[HordeDefense] Sent " << enemies.size() << " enemies and full state to player " << cid << std::endl;
}

// ============================================================================
// Helper Functions for Player Status
// ============================================================================

int HordeDefenseManager::getAlivePlayers() const {
    int count = 0;
    for (const auto& [cid, alive] : playerAlive) {
        if (alive) count++;
    }
    return count;
}

bool HordeDefenseManager::allPlayersDead(const std::map<int32_t, phisics::Entity>& players) const {
    for (const auto& [cid, player] : players) {
        if (player.life > 0) {
            return false;  // Found at least one alive player
        }
    }
    return true;  // All players are dead
}

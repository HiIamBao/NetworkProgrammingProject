#include "BossFightManager.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

using namespace BossFight;

// ============================================================================
// Constructor & Destructor
// ============================================================================

BossFightManager::BossFightManager() 
    : currentState(BossFightState::WAITING)
    , nextMinionId(1)
    , matchTime(0.0f)
    , spawnTimer(0.0f)
    , bossUpdateTimer(0.0f)
    , minionUpdateTimer(0.0f)
    , proximityDamageTimer(0.0f)
    , proximityDamageRadius(3.0f)  // Default 3 tiles
    , proximityDamageAmount(5)     // 5 damage per tick
    , bossPathIndex(0)
    , pathRecalcTimer(0.0f)
    , currentMap(nullptr)
{
    std::random_device rd;
    rng.seed(rd());
}

BossFightManager::~BossFightManager() {
    cleanup();
}

// ============================================================================
// Initialization & Cleanup
// ============================================================================

void BossFightManager::initialize() {
    reset();
    std::cout << "[BossFight] Manager initialized" << std::endl;
}

void BossFightManager::cleanup() {
    minions.clear();
    playerAlive.clear();
    playerDamageDealt.clear();
}

void BossFightManager::reset() {
    currentState = BossFightState::WAITING;
    boss = Boss();
    boss.isAlive = false;
    minions.clear();
    nextMinionId = 1;
    playerAlive.clear();
    playerDamageDealt.clear();
    matchTime = 0.0f;
    spawnTimer = 0.0f;
    bossUpdateTimer = 0.0f;
    minionUpdateTimer = 0.0f;
}

// ============================================================================
// Main Update Loop
// ============================================================================

void BossFightManager::update(float deltaTime, std::map<int32_t, phisics::Entity>& players, phisics::MapData* mapData) {
    currentMap = mapData;  // Store for collision detection
    
    switch (currentState) {
        case BossFightState::WAITING:
            // Do nothing, waiting for start
            break;
            
        case BossFightState::BOSS_SPAWNING:
            spawnTimer += deltaTime;
            if (spawnTimer >= BOSS_SPAWN_DURATION) {
                setState(BossFightState::BOSS_ACTIVE);
                std::cout << "[BossFight] Boss is now active!" << std::endl;
            }
            break;
            
        case BossFightState::BOSS_ACTIVE:
            matchTime += deltaTime;
            updateBoss(deltaTime, players);
            updateMinions(deltaTime, players);
            updateProximityDamage(deltaTime, players);  // NEW: Proximity damage
            checkBossPlayerCollision(players, deltaTime);  // NEW: Collision damage
            removeDeadMinions();
            
            // Broadcast updates at 10Hz
            bossUpdateTimer += deltaTime;
            if (bossUpdateTimer >= 0.1f) {
                broadcastBossUpdate();
                bossUpdateTimer = 0.0f;
            }
            
            minionUpdateTimer += deltaTime;
            if (minionUpdateTimer >= 0.1f && !minions.empty()) {
                broadcastMinionUpdates();
                minionUpdateTimer = 0.0f;
            }
            
            // Check victory condition
            if (!boss.isAlive) {
                endMatch(true);
            }
            
            // Check defeat condition
            if (allPlayersDead()) {
                endMatch(false);
            }
            break;
            
        case BossFightState::BOSS_DEFEATED:
        case BossFightState::PLAYERS_DEFEATED:
            // Match ended, no updates needed
            break;
    }
}

// ============================================================================
// Game State Management
// ============================================================================

void BossFightManager::setState(BossFightState newState) {
    currentState = newState;
    broadcastStateUpdate();
}

void BossFightManager::startMatch() {
    std::cout << "[BossFight] Starting match..." << std::endl;
    
    reset();
    matchTime = 0.0f;
    
    // Initialize all players as alive
    for (auto& pair : playerAlive) {
        pair.second = true;
    }
    
    // Spawn boss near a random player spawn point (very close for debugging)
    glm::vec2 bossSpawnPos = getRandomSpawnPosition();
    // Offset boss just 2 tiles away from player spawn for testing
    bossSpawnPos.x += 2.0f;
    bossSpawnPos.y += 2.0f;
    
    setState(BossFightState::BOSS_SPAWNING);
    spawnBoss(bossSpawnPos);
    
    std::cout << "[BossFight] Boss spawned at (" << bossSpawnPos.x << ", " << bossSpawnPos.y << ")" << std::endl;
}

void BossFightManager::endMatch(bool victory) {
    if (victory) {
        setState(BossFightState::BOSS_DEFEATED);
        std::cout << "[BossFight] VICTORY! Boss defeated!" << std::endl;
    } else {
        setState(BossFightState::PLAYERS_DEFEATED);
        std::cout << "[BossFight] DEFEAT! All players died!" << std::endl;
    }
    
    // Find MVP (most damage dealt)
    int32_t mvpCid = -1;
    int maxDamage = 0;
    for (const auto& pair : playerDamageDealt) {
        if (pair.second > maxDamage) {
            maxDamage = pair.second;
            mvpCid = pair.first;
        }
    }
    
    // Broadcast match end
    BossFightMatchEndData endData;
    endData.victory = victory;
    endData.matchDuration = matchTime;
    endData.mvpPlayerId = mvpCid;
    endData.mvpDamage = maxDamage;
    endData.totalPlayerDeaths = 0;
    
    // Count total deaths
    for (const auto& pair : playerAlive) {
        if (!pair.second) {
            endData.totalPlayerDeaths++;
        }
    }
    
    // MVP name will be filled by server from player data
    endData.mvpPlayerName[0] = '\0';
    
    if (broadcastCallback) {
        Packet p;
        p.header = headerBossFightMatchEnd;
        p.cid = 0;
        broadcastCallback(p, &endData, sizeof(endData), true);
    }
}

// ============================================================================
// Boss Management
// ============================================================================

void BossFightManager::spawnBoss(glm::vec2 position) {
    boss.bossId = 1;
    boss.type = BossType::GIANT_DEMON;
    // Use the provided position (will be near player)
    boss.position = position;
    boss.velocity = glm::vec2(0, 0);
    boss.currentPhase = BossPhase::PHASE_1;
    boss.isAlive = true;
    
    BossStats stats = BossStats::getStats(BossType::GIANT_DEMON);
    boss.health = stats.baseHealth;
    boss.maxHealth = stats.baseHealth;
    boss.speed = stats.baseSpeed;
    boss.baseDamage = stats.baseDamage;
    boss.currentTargetId = -1;
    boss.lastAttackTime = 0.0f;
    boss.nextAttackTimer = 2.0f;
    boss.nextAttackType = BossAttackType::MELEE;
    
    std::cout << "[BossFight] Boss spawned at (" << position.x << ", " << position.y << ")" << std::endl;
    
    // Broadcast boss spawn
    BossFightBossSpawnData spawnData;
    spawnData.bossId = boss.bossId;
    spawnData.bossType = (int)boss.type;
    spawnData.posX = boss.position.x;
    spawnData.posY = boss.position.y;
    spawnData.health = boss.health;
    spawnData.maxHealth = boss.maxHealth;
    spawnData.speed = boss.speed;
    
    if (broadcastCallback) {
        Packet p;
        p.header = headerBossFightBossSpawn;
        p.cid = 0;
        broadcastCallback(p, &spawnData, sizeof(spawnData), true);
    }
}

void BossFightManager::updateBoss(float deltaTime, const std::map<int32_t, phisics::Entity>& players) {
    if (!boss.isAlive) {
        std::cout << "[BossFight] Boss not alive, skipping update" << std::endl;
        return;
    }
    
    // Update phase based on health
    updateBossPhase();
    
    // Select target if needed
    if (boss.currentTargetId == -1 || !isPlayerAlive(boss.currentTargetId)) {
        selectBossTarget(players);
        std::cout << "[BossFight] Boss target selected: " << boss.currentTargetId << std::endl;
    }
    
    // Move towards target with pathfinding
    moveBossWithPathfinding(deltaTime, players, currentMap);
    
    // Update attack timer
    boss.nextAttackTimer -= deltaTime;
    if (boss.nextAttackTimer <= 0.0f) {
        executeBossAttack(const_cast<std::map<int32_t, phisics::Entity>&>(players));
    }
}

bool BossFightManager::damageBoss(int damage, int32_t attackerCid) {
    if (!boss.isAlive) return false;
    
    boss.health -= damage;
    
    // Track player damage
    playerDamageDealt[attackerCid] += damage;
    
    std::cout << "[BossFight] Boss took " << damage << " damage from player " << attackerCid 
              << " (HP: " << boss.health << "/" << boss.maxHealth << ")" << std::endl;
    
    if (boss.health <= 0) {
        boss.health = 0;
        boss.isAlive = false;
        
        std::cout << "[BossFight] Boss defeated!" << std::endl;
        
        // Broadcast boss death
        BossFightBossDeathData deathData;
        deathData.bossId = boss.bossId;
        deathData.lastHitPlayerCid = attackerCid;
        deathData.posX = boss.position.x;
        deathData.posY = boss.position.y;
        
        if (broadcastCallback) {
            Packet p;
            p.header = headerBossFightBossDeath;
            p.cid = 0;
            broadcastCallback(p, &deathData, sizeof(deathData), true);
        }
        
        return true;
    }
    
    return false;
}

void BossFightManager::updateBossPhase() {
    float healthPercent = getBossHealthPercent();
    
    BossPhase newPhase = boss.currentPhase;
    
    if (healthPercent <= 0.4f) {
        newPhase = BossPhase::PHASE_3;
    } else if (healthPercent <= 0.7f) {
        newPhase = BossPhase::PHASE_2;
    }
    
    if (newPhase != boss.currentPhase) {
        boss.currentPhase = newPhase;
        std::cout << "[BossFight] Boss entered Phase " << ((int)newPhase + 1) << "!" << std::endl;
        
        // Increase boss speed in later phases
        if (newPhase == BossPhase::PHASE_2) {
            boss.speed *= 1.2f;
        } else if (newPhase == BossPhase::PHASE_3) {
            boss.speed *= 1.15f;  // Cumulative 1.38x speed
        }
    }
}

// ============================================================================
// Boss AI & Attacks
// ============================================================================

void BossFightManager::selectBossTarget(const std::map<int32_t, phisics::Entity>& players) {
    // Find nearest alive player
    int32_t nearestCid = findNearestPlayer(boss.position, players);
    if (nearestCid != -1) {
        boss.currentTargetId = nearestCid;
    }
}

void BossFightManager::moveBossTowardsTarget(float deltaTime, const std::map<int32_t, phisics::Entity>& players) {
    if (boss.currentTargetId == -1) return;
    
    auto it = players.find(boss.currentTargetId);
    if (it == players.end()) return;
    
    const phisics::Entity& target = it->second;
    glm::vec2 direction = target.pos - boss.position;
    float distance = glm::length(direction);
    
    if (distance > 0.1f) {
        direction = glm::normalize(direction);
        boss.velocity = direction * boss.speed;
        boss.position += boss.velocity * deltaTime;
    } else {
        boss.velocity = glm::vec2(0, 0);
    }
}

void BossFightManager::executeBossAttack(std::map<int32_t, phisics::Entity>& players) {
    // Choose attack based on phase and range
    std::uniform_int_distribution<int> dist(0, 100);
    int roll = dist(rng);
    
    BossAttackType attackType = BossAttackType::MELEE;
    
    if (boss.currentPhase == BossPhase::PHASE_1) {
        attackType = BossAttackType::MELEE;
    } else if (boss.currentPhase == BossPhase::PHASE_2) {
        if (roll < 60) {
            attackType = BossAttackType::MELEE;
        } else {
            attackType = BossAttackType::AOE_SLAM;
        }
    } else if (boss.currentPhase == BossPhase::PHASE_3) {
        if (roll < 40) {
            attackType = BossAttackType::MELEE;
        } else if (roll < 70) {
            attackType = BossAttackType::AOE_SLAM;
        } else if (roll < 90) {
            attackType = BossAttackType::CHARGE;
        } else {
            attackType = BossAttackType::SUMMON_MINIONS;
        }
    }
    
    // Execute the chosen attack
    switch (attackType) {
        case BossAttackType::MELEE:
            performMeleeAttack(players);
            boss.nextAttackTimer = 2.5f;
            break;
        case BossAttackType::AOE_SLAM:
            performAOESlam(players);
            boss.nextAttackTimer = 4.0f;
            break;
        case BossAttackType::CHARGE:
            performCharge(players);
            boss.nextAttackTimer = 5.0f;
            break;
        case BossAttackType::SUMMON_MINIONS:
            summonMinions();
            boss.nextAttackTimer = 8.0f;
            break;
    }
}

void BossFightManager::performMeleeAttack(std::map<int32_t, phisics::Entity>& players) {
    if (boss.currentTargetId == -1) return;
    
    auto it = players.find(boss.currentTargetId);
    if (it == players.end()) return;
    
    phisics::Entity& target = it->second;
    float distance = distanceToPlayer(boss.position, target.pos);
    
    if (distance <= BOSS_ATTACK_RANGE) {
        std::cout << "[BossFight] Boss performs MELEE attack on player " << boss.currentTargetId << std::endl;
        
        // Apply damage
        applyDamageToPlayer(boss.currentTargetId, target, boss.baseDamage, BossAttackType::MELEE, glm::vec2(0, 0));
        
        // Broadcast attack
        BossFightBossAttackData attackData;
        attackData.bossId = boss.bossId;
        attackData.attackType = (int)BossAttackType::MELEE;
        attackData.attackPosX = boss.position.x;
        attackData.attackPosY = boss.position.y;
        attackData.targetCid = boss.currentTargetId;
        attackData.damage = boss.baseDamage;
        
        if (broadcastCallback) {
            Packet p;
            p.header = headerBossFightBossAttack;
            p.cid = 0;
            broadcastCallback(p, &attackData, sizeof(attackData), true);
        }
    }
}

void BossFightManager::performAOESlam(std::map<int32_t, phisics::Entity>& players) {
    std::cout << "[BossFight] Boss performs AOE SLAM!" << std::endl;
    
    // Broadcast attack first (for animation)
    BossFightBossAttackData attackData;
    attackData.bossId = boss.bossId;
    attackData.attackType = (int)BossAttackType::AOE_SLAM;
    attackData.attackPosX = boss.position.x;
    attackData.attackPosY = boss.position.y;
    attackData.targetCid = -1;
    attackData.damage = AOE_SLAM_DAMAGE;
    
    if (broadcastCallback) {
        Packet p;
        p.header = headerBossFightBossAttack;
        p.cid = 0;
        broadcastCallback(p, &attackData, sizeof(attackData), true);
    }
    
    // Damage all players in radius
    for (auto& pair : players) {
        if (!isPlayerAlive(pair.first)) continue;
        
        float distance = distanceToPlayer(boss.position, pair.second.pos);
        if (distance <= AOE_SLAM_RADIUS) {
            glm::vec2 knockback = glm::normalize(pair.second.pos - boss.position) * 5.0f;
            applyDamageToPlayer(pair.first, pair.second, AOE_SLAM_DAMAGE, BossAttackType::AOE_SLAM, knockback);
        }
    }
}

void BossFightManager::performCharge(std::map<int32_t, phisics::Entity>& players) {
    if (boss.currentTargetId == -1) return;
    
    auto it = players.find(boss.currentTargetId);
    if (it == players.end()) return;
    
    std::cout << "[BossFight] Boss performs CHARGE attack!" << std::endl;
    
    phisics::Entity& target = it->second;
    glm::vec2 direction = target.pos - boss.position;
    float distance = glm::length(direction);
    
    if (distance > 0.1f && distance <= CHARGE_RANGE) {
        direction = glm::normalize(direction);
        
        // Move boss quickly towards target
        boss.position += direction * 10.0f;
        
        // Check if hit target
        if (distanceToPlayer(boss.position, target.pos) <= 2.0f) {
            glm::vec2 knockback = direction * 8.0f;
            applyDamageToPlayer(boss.currentTargetId, target, CHARGE_DAMAGE, BossAttackType::CHARGE, knockback);
        }
        
        // Broadcast attack
        BossFightBossAttackData attackData;
        attackData.bossId = boss.bossId;
        attackData.attackType = (int)BossAttackType::CHARGE;
        attackData.attackPosX = target.pos.x;
        attackData.attackPosY = target.pos.y;
        attackData.targetCid = boss.currentTargetId;
        attackData.damage = CHARGE_DAMAGE;
        
        if (broadcastCallback) {
            Packet p;
            p.header = headerBossFightBossAttack;
            p.cid = 0;
            broadcastCallback(p, &attackData, sizeof(attackData), true);
        }
    }
}

void BossFightManager::summonMinions() {
    std::cout << "[BossFight] Boss summons minions!" << std::endl;
    
    // Spawn minions around boss
    for (int i = 0; i < MINION_SPAWN_COUNT; i++) {
        float angle = (3.14159f * 2.0f * i) / MINION_SPAWN_COUNT;
        glm::vec2 offset(std::cos(angle) * 3.0f, std::sin(angle) * 3.0f);
        spawnMinion(boss.position + offset);
    }
    
    // Broadcast attack
    BossFightBossAttackData attackData;
    attackData.bossId = boss.bossId;
    attackData.attackType = (int)BossAttackType::SUMMON_MINIONS;
    attackData.attackPosX = boss.position.x;
    attackData.attackPosY = boss.position.y;
    attackData.targetCid = -1;
    attackData.damage = 0;
    
    if (broadcastCallback) {
        Packet p;
        p.header = headerBossFightBossAttack;
        p.cid = 0;
        broadcastCallback(p, &attackData, sizeof(attackData), true);
    }
}

// ============================================================================
// Minion Management
// ============================================================================

void BossFightManager::spawnMinion(glm::vec2 position) {
    Minion minion;
    minion.minionId = nextMinionId++;
    minion.position = position;
    minion.velocity = glm::vec2(0, 0);
    minion.health = 50;
    minion.maxHealth = 50;
    minion.speed = 6.0f;
    minion.damage = 10;
    minion.targetPlayerId = -1;
    minion.isAlive = true;
    minion.lastAttackTime = 0.0f;
    
    minions.push_back(minion);
    
    // Broadcast minion spawn
    BossFightMinionSpawnData spawnData;
    spawnData.minionId = minion.minionId;
    spawnData.posX = minion.position.x;
    spawnData.posY = minion.position.y;
    spawnData.health = minion.health;
    spawnData.maxHealth = minion.maxHealth;
    
    if (broadcastCallback) {
        Packet p;
        p.header = headerBossFightMinionSpawn;
        p.cid = 0;
        broadcastCallback(p, &spawnData, sizeof(spawnData), true);
    }
}

void BossFightManager::updateMinions(float deltaTime, std::map<int32_t, phisics::Entity>& players) {
    for (auto& minion : minions) {
        if (!minion.isAlive) continue;
        
        // Select target if needed
        if (minion.targetPlayerId == -1 || !isPlayerAlive(minion.targetPlayerId)) {
            minion.targetPlayerId = findNearestPlayer(minion.position, players);
        }
        
        if (minion.targetPlayerId == -1) continue;
        
        auto it = players.find(minion.targetPlayerId);
        if (it == players.end()) continue;
        
        phisics::Entity& target = it->second;
        glm::vec2 direction = target.pos - minion.position;
        float distance = glm::length(direction);
        
        // Move towards target
        if (distance > MINION_ATTACK_RANGE) {
            if (distance > 0.1f) {
                direction = glm::normalize(direction);
                minion.velocity = direction * minion.speed;
                minion.position += minion.velocity * deltaTime;
            }
        } else {
            // Attack target
            minion.velocity = glm::vec2(0, 0);
            minion.lastAttackTime += deltaTime;
            
            if (minion.lastAttackTime >= MINION_ATTACK_COOLDOWN) {
                applyDamageToPlayer(minion.targetPlayerId, target, minion.damage, BossAttackType::MELEE, glm::vec2(0, 0));
                minion.lastAttackTime = 0.0f;
            }
        }
    }
}

void BossFightManager::removeDeadMinions() {
    minions.erase(
        std::remove_if(minions.begin(), minions.end(),
            [](const Minion& m) { return !m.isAlive; }),
        minions.end()
    );
}

bool BossFightManager::damageMinion(int32_t minionId, int damage, int32_t attackerCid) {
    for (auto& minion : minions) {
        if (minion.minionId == minionId && minion.isAlive) {
            minion.health -= damage;
            
            if (minion.health <= 0) {
                minion.health = 0;
                minion.isAlive = false;
                
                std::cout << "[BossFight] Minion " << minionId << " killed by player " << attackerCid << std::endl;
                
                // Broadcast minion death
                BossFightMinionDeathData deathData;
                deathData.minionId = minionId;
                deathData.killerCid = attackerCid;
                deathData.posX = minion.position.x;
                deathData.posY = minion.position.y;
                
                if (broadcastCallback) {
                    Packet p;
                    p.header = headerBossFightMinionDeath;
                    p.cid = 0;
                    broadcastCallback(p, &deathData, sizeof(deathData), true);
                }
                
                return true;
            }
            return false;
        }
    }
    return false;
}

// ============================================================================
// Player Management
// ============================================================================

void BossFightManager::addPlayer(int32_t cid) {
    playerAlive[cid] = true;
    playerDamageDealt[cid] = 0;
    std::cout << "[BossFight] Player " << cid << " added to Boss Fight" << std::endl;
}

void BossFightManager::removePlayer(int32_t cid) {
    playerAlive.erase(cid);
    playerDamageDealt.erase(cid);
    std::cout << "[BossFight] Player " << cid << " removed from Boss Fight" << std::endl;
}

void BossFightManager::respawnPlayer(int32_t cid, phisics::Entity& player) {
    glm::vec2 spawnPos = getSafeRespawnPosition();
    player.pos = spawnPos;
    player.lastPos = spawnPos;
    player.life = player.maxLife;
    playerAlive[cid] = true;
    
    std::cout << "[BossFight] Player " << cid << " respawned at (" << spawnPos.x << ", " << spawnPos.y << ")" << std::endl;
    
    // Broadcast respawn
    BossFightPlayerRespawnData respawnData;
    respawnData.cid = cid;
    respawnData.posX = spawnPos.x;
    respawnData.posY = spawnPos.y;
    
    if (broadcastCallback) {
        Packet p;
        p.header = headerBossFightPlayerRespawn;
        p.cid = 0;
        broadcastCallback(p, &respawnData, sizeof(respawnData), true);
    }
}

void BossFightManager::markPlayerDead(int32_t cid) {
    playerAlive[cid] = false;
    std::cout << "[BossFight] Player " << cid << " died" << std::endl;
}

bool BossFightManager::isPlayerAlive(int32_t cid) const {
    auto it = playerAlive.find(cid);
    return (it != playerAlive.end() && it->second);
}

bool BossFightManager::allPlayersDead() const {
    for (const auto& pair : playerAlive) {
        if (pair.second) return false;
    }
    return !playerAlive.empty();
}

int BossFightManager::getAlivePlayers() const {
    int count = 0;
    for (const auto& pair : playerAlive) {
        if (pair.second) count++;
    }
    return count;
}

// ============================================================================
// Network Broadcasting
// ============================================================================

void BossFightManager::broadcastStateUpdate() {
    BossFightStateUpdateData stateData;
    stateData.gameState = (int)currentState;
    stateData.bossHealth = boss.health;
    stateData.bossMaxHealth = boss.maxHealth;
    stateData.bossPhase = (int)boss.currentPhase;
    stateData.playersAlive = getAlivePlayers();
    stateData.matchTime = matchTime;
    
    if (broadcastCallback) {
        Packet p;
        p.header = headerBossFightStateUpdate;
        p.cid = 0;
        broadcastCallback(p, &stateData, sizeof(stateData), true);
    }
}

void BossFightManager::broadcastBossUpdate() {
    BossFightBossUpdateData updateData;
    updateData.bossId = boss.bossId;
    updateData.posX = boss.position.x;
    updateData.posY = boss.position.y;
    updateData.velX = boss.velocity.x;
    updateData.velY = boss.velocity.y;
    updateData.health = boss.health;
    updateData.currentPhase = (int)boss.currentPhase;
    updateData.targetPlayerId = boss.currentTargetId;
    
    if (broadcastCallback) {
        Packet p;
        p.header = headerBossFightBossUpdate;
        p.cid = 0;
        broadcastCallback(p, &updateData, sizeof(updateData), false);  // Unreliable
    }
}

void BossFightManager::broadcastMinionUpdates() {
    for (const auto& minion : minions) {
        if (!minion.isAlive) continue;
        
        BossFightMinionUpdateData updateData;
        updateData.minionId = minion.minionId;
        updateData.posX = minion.position.x;
        updateData.posY = minion.position.y;
        updateData.health = minion.health;
        updateData.targetPlayerId = minion.targetPlayerId;
        
        if (broadcastCallback) {
            Packet p;
            p.header = headerBossFightMinionUpdate;
            p.cid = 0;
            broadcastCallback(p, &updateData, sizeof(updateData), false);  // Unreliable
        }
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

glm::vec2 BossFightManager::getRandomSpawnPosition() {
    static glm::vec2 spawnPositions[] = {
        {30.0f, 10.0f},  // North
        {30.0f, 50.0f},  // South
        {10.0f, 30.0f},  // West
        {50.0f, 30.0f}   // East
    };
    
    std::uniform_int_distribution<int> dist(0, 3);
    return spawnPositions[dist(rng)];
}

glm::vec2 BossFightManager::getSafeRespawnPosition() {
    // Spawn far from boss
    glm::vec2 spawn = getRandomSpawnPosition();
    
    // Ensure at least 15 tiles away from boss
    while (distanceToPlayer(spawn, boss.position) < 15.0f) {
        spawn = getRandomSpawnPosition();
    }
    
    return spawn;
}

int32_t BossFightManager::findNearestPlayer(glm::vec2 position, const std::map<int32_t, phisics::Entity>& players) {
    int32_t nearestCid = -1;
    float minDistance = 999999.0f;
    
    for (const auto& pair : players) {
        if (!isPlayerAlive(pair.first)) continue;
        
        float distance = distanceToPlayer(position, pair.second.pos);
        if (distance < minDistance) {
            minDistance = distance;
            nearestCid = pair.first;
        }
    }
    
    return nearestCid;
}

float BossFightManager::distanceToPlayer(glm::vec2 pos1, glm::vec2 pos2) {
    return glm::length(pos2 - pos1);
}

float BossFightManager::getBossHealthPercent() const {
    if (boss.maxHealth == 0) return 0.0f;
    return boss.health / boss.maxHealth;
}

void BossFightManager::applyDamageToPlayer(int32_t cid, phisics::Entity& player, int damage, BossAttackType attackType, glm::vec2 knockback) {
    player.life -= damage;
    
    if (player.life <= 0) {
        player.life = 0;
        markPlayerDead(cid);
    }
    
    // Send damage packet to player
    BossFightPlayerDamageData damageData;
    damageData.cid = cid;
    damageData.damage = damage;
    damageData.attackType = (int)attackType;
    damageData.knockbackX = knockback.x;
    damageData.knockbackY = knockback.y;
    
    if (sendToPlayerCallback) {
        Packet p;
        p.header = headerBossFightPlayerDamage;
        p.cid = 0;
        sendToPlayerCallback(cid, p, &damageData, sizeof(damageData), true);
    }
}

// ============================================================================
// Pathfinding & Collision
// ============================================================================

struct PathNode {
    int x, y;
    float g, h, f;
    PathNode* parent;
    
    PathNode(int x, int y) : x(x), y(y), g(0), h(0), f(0), parent(nullptr) {}
};

bool BossFightManager::isWalkable(int x, int y, phisics::MapData* mapData) {
    if (!mapData || !mapData->data) return true;
    if (x < 0 || y < 0 || x >= mapData->w || y >= mapData->h) return false;
    
    // Check if tile is collidable (wall)
    phisics::BlockInfo& block = mapData->get(x, y);
    return !block.isCollidable();
}

std::vector<glm::vec2> BossFightManager::findPath(glm::vec2 start, glm::vec2 end, phisics::MapData* mapData) {
    std::vector<glm::vec2> path;
    
    if (!mapData || !mapData->data) {
        // No map data - direct line
        path.push_back(end);
        return path;
    }
    
    // Convert world coordinates to tile coordinates
    int startX = (int)start.x;
    int startY = (int)start.y;
    int endX = (int)end.x;
    int endY = (int)end.y;
    
    // Simple BFS pathfinding
    std::vector<PathNode*> open;
    std::vector<PathNode*> closed;
    
    PathNode* startNode = new PathNode(startX, startY);
    open.push_back(startNode);
    
    PathNode* endNode = nullptr;
    int maxIterations = 500;  // Prevent infinite loops
    int iterations = 0;
    
    while (!open.empty() && iterations < maxIterations) {
        iterations++;
        
        // Find node with lowest f score
        int lowestIndex = 0;
        for (int i = 1; i < open.size(); i++) {
            if (open[i]->f < open[lowestIndex]->f) {
                lowestIndex = i;
            }
        }
        
        PathNode* current = open[lowestIndex];
        open.erase(open.begin() + lowestIndex);
        closed.push_back(current);
        
        // Check if we reached the goal
        if (abs(current->x - endX) <= 1 && abs(current->y - endY) <= 1) {
            endNode = current;
            break;
        }
        
        // Check 8 neighbors (including diagonals)
        int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
        
        for (int i = 0; i < 8; i++) {
            int newX = current->x + dx[i];
            int newY = current->y + dy[i];
            
            // Check if walkable
            if (!isWalkable(newX, newY, mapData)) continue;
            
            // Check if already in closed list
            bool inClosed = false;
            for (PathNode* node : closed) {
                if (node->x == newX && node->y == newY) {
                    inClosed = true;
                    break;
                }
            }
            if (inClosed) continue;
            
            // Calculate scores
            float gCost = current->g + (abs(dx[i]) + abs(dy[i]) == 2 ? 1.414f : 1.0f);  // Diagonal cost
            float hCost = abs(newX - endX) + abs(newY - endY);  // Manhattan distance
            float fCost = gCost + hCost;
            
            // Check if already in open list
            PathNode* existingNode = nullptr;
            for (PathNode* node : open) {
                if (node->x == newX && node->y == newY) {
                    existingNode = node;
                    break;
                }
            }
            
            if (existingNode) {
                // Update if this path is better
                if (gCost < existingNode->g) {
                    existingNode->g = gCost;
                    existingNode->f = fCost;
                    existingNode->parent = current;
                }
            } else {
                // Add new node
                PathNode* newNode = new PathNode(newX, newY);
                newNode->g = gCost;
                newNode->h = hCost;
                newNode->f = fCost;
                newNode->parent = current;
                open.push_back(newNode);
            }
        }
    }
    
    // Reconstruct path
    if (endNode) {
        PathNode* current = endNode;
        while (current != nullptr) {
            path.insert(path.begin(), glm::vec2(current->x + 0.5f, current->y + 0.5f));  // Center of tile
            current = current->parent;
        }
    } else {
        // No path found - move directly
        path.push_back(end);
    }
    
    // Clean up
    for (PathNode* node : open) delete node;
    for (PathNode* node : closed) delete node;
    
    return path;
}

void BossFightManager::moveBossWithPathfinding(float deltaTime, const std::map<int32_t, phisics::Entity>& players, phisics::MapData* mapData) {
    if (boss.currentTargetId == -1) {
        std::cout << "[BossFight] No target, skipping movement" << std::endl;
        return;
    }
    
    auto it = players.find(boss.currentTargetId);
    if (it == players.end()) {
        std::cout << "[BossFight] Target player not found" << std::endl;
        return;
    }
    
    const phisics::Entity& target = it->second;
    currentMap = mapData;
    
    std::cout << "[BossFight] Moving boss from (" << boss.position.x << ", " << boss.position.y 
              << ") towards player at (" << target.pos.x << ", " << target.pos.y << ")" << std::endl;
    
    // Recalculate path periodically
    pathRecalcTimer -= deltaTime;
    if (pathRecalcTimer <= 0.0f || bossPath.empty()) {
        bossPath = findPath(boss.position, target.pos, mapData);
        bossPathIndex = 0;
        pathRecalcTimer = 1.0f;  // Recalc every 1 second
        std::cout << "[BossFight] Recalculated path, waypoints: " << bossPath.size() << std::endl;
    }
    
    // Follow path
    if (!bossPath.empty() && bossPathIndex < bossPath.size()) {
        glm::vec2 waypoint = bossPath[bossPathIndex];
        glm::vec2 direction = waypoint - boss.position;
        float distance = glm::length(direction);
        
        std::cout << "[BossFight] Following waypoint " << bossPathIndex << "/" << bossPath.size() 
                  << " at (" << waypoint.x << ", " << waypoint.y << "), distance: " << distance << std::endl;
        
        if (distance < 0.3f) {
            // Reached waypoint, move to next
            bossPathIndex++;
            std::cout << "[BossFight] Reached waypoint, moving to next" << std::endl;
        } else {
            // Move towards waypoint
            direction = glm::normalize(direction);
            glm::vec2 oldPos = boss.position;
            boss.velocity = direction * boss.speed;
            boss.position += boss.velocity * deltaTime;
            
            std::cout << "[BossFight] Moved boss to (" << boss.position.x << ", " << boss.position.y 
                      << "), velocity: " << boss.speed << std::endl;
            
            // Resolve collisions with map
            if (mapData) {
                resolveBossCollision(mapData);
            }
        }
    } else {
        std::cout << "[BossFight] No valid path or reached end" << std::endl;
    }
}

void BossFightManager::resolveBossCollision(phisics::MapData* mapData) {
    if (!mapData || !mapData->data) return;
    
    // Boss is 2x2 tiles
    glm::vec2 bossSize(2.0f, 2.0f);
    
    // Check collision with map tiles
    int minX = (int)boss.position.x;
    int maxX = (int)(boss.position.x + bossSize.x);
    int minY = (int)boss.position.y;
    int maxY = (int)(boss.position.y + bossSize.y);
    
    // Clamp to map bounds
    if (boss.position.x < 0) boss.position.x = 0;
    if (boss.position.y < 0) boss.position.y = 0;
    if (boss.position.x + bossSize.x > mapData->w) boss.position.x = mapData->w - bossSize.x;
    if (boss.position.y + bossSize.y > mapData->h) boss.position.y = mapData->h - bossSize.y;
    
    // Simple collision resolution - push back if colliding
    for (int y = minY; y <= maxY && y < mapData->h; y++) {
        for (int x = minX; x <= maxX && x < mapData->w; x++) {
            phisics::BlockInfo& block = mapData->get(x, y);
            if (block.isCollidable()) {
                // Push boss away from wall
                glm::vec2 blockCenter(x + 0.5f, y + 0.5f);
                glm::vec2 bossCenter = boss.position + bossSize * 0.5f;
                glm::vec2 pushDir = bossCenter - blockCenter;
                
                if (glm::length(pushDir) > 0.01f) {
                    pushDir = glm::normalize(pushDir);
                    boss.position += pushDir * 0.1f;  // Push back slightly
                }
            }
        }
    }
}

void BossFightManager::checkBossPlayerCollision(std::map<int32_t, phisics::Entity>& players, float deltaTime) {
    glm::vec2 bossMin = boss.position;
    glm::vec2 bossMax = boss.position + glm::vec2(2.0f, 2.0f);
    
    for (auto& pair : players) {
        if (!isPlayerAlive(pair.first)) continue;
        
        phisics::Entity& player = pair.second;
        glm::vec2 playerMin = player.pos;
        glm::vec2 playerMax = player.pos + player.dimensions;
        
        // AABB collision check
        if (bossMax.x > playerMin.x && bossMin.x < playerMax.x &&
            bossMax.y > playerMin.y && bossMin.y < playerMax.y) {
            
            // Collision detected - push player away and deal damage
            glm::vec2 bossCenter = boss.position + glm::vec2(1.0f, 1.0f);
            glm::vec2 playerCenter = player.pos + player.dimensions * 0.5f;
            glm::vec2 pushDir = playerCenter - bossCenter;
            
            if (glm::length(pushDir) > 0.01f) {
                pushDir = glm::normalize(pushDir);
                
                // Push player
                player.pos += pushDir * 5.0f * deltaTime;
                
                // Deal contact damage (once per second)
                if (player.hitTime <= 0.0f) {
                    applyDamageToPlayer(pair.first, player, 10, BossAttackType::MELEE, pushDir * 2.0f);
                }
            }
        }
    }
}

void BossFightManager::updateProximityDamage(float deltaTime, std::map<int32_t, phisics::Entity>& players) {
    proximityDamageTimer -= deltaTime;
    
    if (proximityDamageTimer <= 0.0f) {
        proximityDamageTimer = 1.0f;  // Damage every 1 second
        
        glm::vec2 bossCenter = boss.position + glm::vec2(1.0f, 1.0f);
        
        for (auto& pair : players) {
            if (!isPlayerAlive(pair.first)) continue;
            
            phisics::Entity& player = pair.second;
            glm::vec2 playerCenter = player.pos + player.dimensions * 0.5f;
            float distance = glm::length(playerCenter - bossCenter);
            
            // Check if player is within damage radius
            if (distance <= proximityDamageRadius) {
                applyDamageToPlayer(pair.first, player, proximityDamageAmount, 
                                   BossAttackType::AOE_SLAM, glm::vec2(0, 0));
                
                std::cout << "[BossFight] Proximity damage: " << proximityDamageAmount 
                         << " to player " << pair.first 
                         << " (distance: " << distance << ")" << std::endl;
            }
        }
    }
}

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
    , matchTime(0.0f)
    , spawnTimer(0.0f)
    , bossUpdateTimer(0.0f)
    , proximityDamageTimer(0.0f)
    , proximityDamageRadius(BOSS_CONTACT_RADIUS)  // Default 7 tiles
    , proximityDamageAmount(BOSS_CONTACT_DAMAGE_PHASE1)  // Start with Phase 1 damage
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
    playerAlive.clear();
    playerDamageDealt.clear();
}

void BossFightManager::reset() {
    currentState = BossFightState::WAITING;
    boss = Boss();
    boss.isAlive = false;
    playerAlive.clear();
    playerDamageDealt.clear();
    bossBullets.clear();  // Clear any active boss projectiles
    playerBullets.clear(); // Clear any active player bullets
    matchTime = 0.0f;
    spawnTimer = 0.0f;
    bossUpdateTimer = 0.0f;
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
            updateProximityDamage(deltaTime, players);
            checkBossPlayerCollision(players, deltaTime);
            updateBossBullets(deltaTime, players);  // Update circle spray bullets
            updatePlayerBullets(deltaTime);         // Update player bullets for boss damage
            
            // Broadcast updates at 20Hz (0.05s) for smoother movement
            bossUpdateTimer += deltaTime;
            if (bossUpdateTimer >= 0.05f) {
                broadcastBossUpdate();
                bossUpdateTimer = 0.0f;
            }
            
            // Check victory condition
            if (!boss.isAlive) {
                endMatch(true, players);
            }
            
            // Check defeat condition
            if (allPlayersDead()) {
                endMatch(false, players);
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

glm::vec2 BossFightManager::findValidBossSpawnPosition(phisics::MapData* mapData) {
    if (!mapData || !mapData->data) {
        return getRandomSpawnPosition();
    }
    
    // Boss is 5x5 tiles, need to check 5x5 area
    const int maxAttempts = 100;
    std::uniform_int_distribution<int> xDist(3, mapData->w - 6);  // Leave margin for 5x boss size
    std::uniform_int_distribution<int> yDist(3, mapData->h - 6);
    
    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        int x = xDist(rng);
        int y = yDist(rng);
        
        // Check if 5x5 area is walkable
        bool valid = true;
        for (int dy = -2; dy <= 2 && valid; dy++) {
            for (int dx = -2; dx <= 2 && valid; dx++) {
                if (!isWalkable(x + dx, y + dy, mapData)) {
                    valid = false;
                }
            }
        }
        
        if (valid) {
            return glm::vec2(x, y);
        }
    }
    
    // Fallback: try center of map
    glm::vec2 centerPos(mapData->w / 2.0f, mapData->h / 2.0f);
    if (isWalkable((int)centerPos.x, (int)centerPos.y, mapData)) {
        return centerPos;
    }
    
    std::cout << "[BossFight] Warning: Could not find valid spawn position, using random position" << std::endl;
    return getRandomSpawnPosition();
}

void BossFightManager::startMatch(phisics::MapData* mapData, int level) {
    std::cout << "[BossFight] Starting match..." << std::endl;
    
    reset();
    matchTime = 0.0f;
    currentMap = mapData;  // Store map data for collision detection
    
    // Initialize all players as alive
    for (auto& pair : playerAlive) {
        pair.second = true;
    }
    
    // Find valid spawn position (not on collidable blocks, within map bounds)
    glm::vec2 bossSpawnPos = findValidBossSpawnPosition(mapData);
    
    setState(BossFightState::BOSS_SPAWNING);
    boss.bossLevel = level;
    spawnBoss(bossSpawnPos);
    
    std::cout << "[BossFight] Boss spawned at (" << bossSpawnPos.x << ", " << bossSpawnPos.y << ")" << std::endl;
}

void BossFightManager::endMatch(bool victory, const std::map<int32_t, phisics::Entity>& players) {
    setState(victory ? BossFightState::BOSS_DEFEATED : BossFightState::PLAYERS_DEFEATED);
    
    // Find MVP (most damage dealt in Boss Fight)
    int32_t mvpCid = -1;
    int maxDamage = 0;
    const char* mvpName = "Unknown";
    
    for (const auto& [cid, player] : players) {
        int damage = getPlayerDamageDealt(cid);
        if (damage > maxDamage) {
            maxDamage = damage;
            mvpCid = cid;
            mvpName = player.name;
        }
    }
    
    // Prepare match end data
    MatchEndData endData;
    endData.winnerCid = mvpCid;
    strncpy(endData.winnerName, mvpName, sizeof(endData.winnerName) - 1);
    endData.winnerName[sizeof(endData.winnerName) - 1] = '\0';
    
    // Set winner stats (damage dealt as "kills" for boss fight)
    endData.winnerKills = maxDamage;
    endData.winnerDeaths = 0;  // Not used in Boss Fight
    endData.totalPlayers = players.size();
    
    // Add player scores to match end data
    int scoreIndex = 0;
    for (const auto& [cid, player] : players) {
        if (scoreIndex >= MAX_SCOREBOARD_PLAYERS) break;
        
        PlayerScore score;
        score.cid = cid;
        strncpy(score.playerName, player.name, sizeof(score.playerName) - 1);
        score.playerName[sizeof(score.playerName) - 1] = '\0';
        score.kills = 0;  // Not used in Boss Fight
        score.deaths = isPlayerAlive(cid) ? 0 : 1;
        score.score = getPlayerDamageDealt(cid);  // Score = total damage to boss
        score.totalDamageDealt = getPlayerDamageDealt(cid);
        endData.scores[scoreIndex++] = score;
    }
    endData.totalPlayers = scoreIndex;
    
    // Debug output
    std::cout << "DEBUG: Boss Fight Match End Data" << std::endl;
    std::cout << "Victory: " << (victory ? "YES" : "NO") << " | Match Duration: " << matchTime << "s" << std::endl;
    std::cout << "MVP CID: " << endData.winnerCid << " Name: " << endData.winnerName << std::endl;
    std::cout << "MVP Damage: " << endData.winnerKills << " | Total Players: " << endData.totalPlayers << std::endl;
    for (int i = 0; i < endData.totalPlayers; i++) {
        const auto& s = endData.scores[i];
        std::cout << " - Player: " << s.playerName << " (CID: " << s.cid << ") "
                  << "Damage: " << s.totalDamageDealt << " | Deaths: " << s.deaths << std::endl;
    }
    
    // Broadcast match end
    if (broadcastCallback) {
        Packet p;
        p.header = headerMatchEnd;
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
  
    boss.health = stats.baseHealth * boss.bossLevel;
    boss.maxHealth = stats.baseHealth * boss.bossLevel;
    
    boss.speed = stats.baseSpeed;
    boss.baseDamage = stats.baseDamage;
    boss.currentTargetId = -1;
    boss.lastAttackTime = 0.0f;
    boss.nextAttackTimer = 2.0f;
    boss.nextAttackType = BossAttackType::MELEE;
    

    std::cout << "[DEBUG][BossFightManager.cpp][256] Boss spawned at (" << position.x << ", " << position.y << ")" << std::endl;
    std::cout << "[DEBUG][BossFightManager.cpp][256] Boss health: " << boss.health << std::endl;
    std::cout << "[DEBUG][BossFightManager.cpp][256] Boss max health: " << boss.maxHealth << std::endl;
    std::cout << "[DEBUG][BossFightManager.cpp][256] Boss level: " << boss.bossLevel << std::endl;
    std::cout << "[DEBUG][BossFightManager.cpp][256] Boss base damage: " << boss.baseDamage << std::endl;
    std::cout << "[DEBUG][BossFightManager.cpp][256] Boss speed: " << boss.speed << std::endl;
    // Broadcast boss spawn
    BossFightBossSpawnData spawnData;
    spawnData.bossId = boss.bossId;
    spawnData.bossType = (int)boss.type;
    spawnData.bossLevel = boss.bossLevel;  // Send boss level for texture selection
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
    
    if (healthPercent <= PHASE_3_THRESHOLD) {
        newPhase = BossPhase::PHASE_3;
    } else if (healthPercent <= PHASE_2_THRESHOLD) {
        newPhase = BossPhase::PHASE_2;
    }
    
    if (newPhase != boss.currentPhase) {
        BossPhase oldPhase = boss.currentPhase;
        boss.currentPhase = newPhase;
        std::cout << "[BossFight] Boss entered Phase " << ((int)newPhase + 1) << "!" << std::endl;
        
        // Update speed and abilities based on phase
        if (newPhase == BossPhase::PHASE_2) {
            boss.speed = PHASE_1_SPEED * PHASE_2_SPEED_MULT;
            boss.skillCooldown = CIRCLE_SPRAY_COOLDOWN_PHASE2;
            boss.nextAttackTimer = 2.0f;
            proximityDamageAmount = BOSS_CONTACT_DAMAGE_PHASE2;
        } else if (newPhase == BossPhase::PHASE_3) {
            boss.speed = PHASE_1_SPEED * PHASE_2_SPEED_MULT * PHASE_3_SPEED_MULT;
            boss.skillCooldown = CIRCLE_SPRAY_COOLDOWN_PHASE3;
            boss.nextAttackTimer = 1.5f;
            proximityDamageAmount = BOSS_CONTACT_DAMAGE_PHASE3;
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
    std::uniform_int_distribution<int> dist(0, 100);
    int roll = dist(rng);
    
    BossAttackType attackType = BossAttackType::MELEE;
    
    if (boss.currentPhase == BossPhase::PHASE_1) {
        // Phase 1: 50% melee, 50% circle spray (Changed for testing/variety)
        if (roll < 50) {
            attackType = BossAttackType::MELEE;
            boss.nextAttackTimer = 2.0f;
        } else {
            attackType = BossAttackType::CIRCLE_SPRAY;
            boss.nextAttackTimer = 2.5f;
        }
    } else if (boss.currentPhase == BossPhase::PHASE_2) {
        // Phase 2: 70% melee, 30% circle spray
        if (roll < 70) {
            attackType = BossAttackType::MELEE;
            boss.nextAttackTimer = 2.0f;
        } else {
            attackType = BossAttackType::CIRCLE_SPRAY;
            boss.nextAttackTimer = 2.0f;
        }
    } else if (boss.currentPhase == BossPhase::PHASE_3) {
        // Phase 3: 50% melee, 50% circle spray
        if (roll < 50) {
            attackType = BossAttackType::MELEE;
            boss.nextAttackTimer = 1.5f;
        } else {
            attackType = BossAttackType::CIRCLE_SPRAY;
            boss.nextAttackTimer = 1.5f;
        }
    }
    
    // Execute the chosen attack
    switch (attackType) {
        case BossAttackType::MELEE:
            performMeleeAttack(players);
            break;
        case BossAttackType::CIRCLE_SPRAY:
            performCircleSprayAttack(players);
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

void BossFightManager::performCircleSprayAttack(std::map<int32_t, phisics::Entity>& players) {
    std::cout << "[BossFight] Boss performs CIRCLE SPRAY attack!" << std::endl;
    
    // Prepare circle spray data
    BossFightCircleSprayData sprayData;
    sprayData.bossId = boss.bossId;
    sprayData.centerX = boss.position.x;
    sprayData.centerY = boss.position.y;
    sprayData.bulletCount = CIRCLE_SPRAY_BULLETS;
    sprayData.bulletSpeed = CIRCLE_SPRAY_SPEED;
    sprayData.damage = boss.baseDamage;
    
    // Spawn actual bullets for server-side collision detection
    float angleStep = 2.0f * 3.14159f / CIRCLE_SPRAY_BULLETS;
    for (int i = 0; i < CIRCLE_SPRAY_BULLETS; i++) {
        float angle = i * angleStep;
        BossBullet bullet;
        bullet.pos = boss.position;
        bullet.velocity = glm::vec2(std::cos(angle), std::sin(angle)) * CIRCLE_SPRAY_SPEED;
        bullet.damage = boss.baseDamage;
        bullet.lifetime = 5.0f;  // 5 seconds lifetime
        bullet.active = true;
        bossBullets.push_back(bullet);
    }
    
    // Broadcast circle spray for client visualization
    if (broadcastCallback) {
        Packet p;
        p.header = headerBossFightCircleSpray;
        p.cid = 0;
        broadcastCallback(p, &sprayData, sizeof(sprayData), true);
    }
    
    // Update last skill time
    boss.lastSkillTime = matchTime;
    boss.isSprayingCircle = false;
}

void BossFightManager::updateBossBullets(float deltaTime, std::map<int32_t, phisics::Entity>& players) {
    const float BULLET_RADIUS = 0.5f;  // Bullet collision radius
    const float PLAYER_RADIUS = 0.8f;  // Player collision radius
    
    for (auto& bullet : bossBullets) {
        if (!bullet.active) continue;
        
        // Update position
        bullet.pos += bullet.velocity * deltaTime;
        bullet.lifetime -= deltaTime;
        
        // Deactivate if lifetime expired
        if (bullet.lifetime <= 0) {
            bullet.active = false;
            continue;
        }
        
        // Check collision with players
        for (auto& [cid, player] : players) {
            if (!isPlayerAlive(cid)) continue;
            
            float dist = glm::length(bullet.pos - player.pos);
            if (dist < BULLET_RADIUS + PLAYER_RADIUS) {
                // Hit player
                applyDamageToPlayer(cid, player, bullet.damage, BossFight::BossAttackType::CIRCLE_SPRAY, glm::vec2(0, 0));
                bullet.active = false;
                std::cout << "[BossFight] Circle spray bullet hit player " << cid << " for " << bullet.damage << " damage" << std::endl;
                break;
            }
        }
    }
    
    // Remove inactive bullets
    bossBullets.erase(
        std::remove_if(bossBullets.begin(), bossBullets.end(),
            [](const BossBullet& b) { return !b.active; }),
        bossBullets.end()
    );
}

void BossFightManager::addPlayerBullet(glm::vec2 pos, glm::vec2 vel, int damage, int32_t cid) {
    PlayerBullet bullet;
    bullet.pos = pos;
    bullet.velocity = vel;
    bullet.damage = damage;
    bullet.cid = cid;
    bullet.lifetime = 2.0f; // Standard bullet lifetime
    bullet.active = true;
    playerBullets.push_back(bullet);
}

void BossFightManager::updatePlayerBullets(float deltaTime) {
    if (!boss.isAlive) return;

    // Boss AABB for collision
    glm::vec2 bossMin = boss.position - glm::vec2(BossFight::BOSS_HITBOX_HALF, BossFight::BOSS_HITBOX_HALF);
    glm::vec2 bossMax = boss.position + glm::vec2(BossFight::BOSS_HITBOX_HALF, BossFight::BOSS_HITBOX_HALF);

    for (auto& bullet : playerBullets) {
        if (!bullet.active) continue;

        // Store old position for continuous collision detection (optional, but good for fast bullets)
        // glm::vec2 oldPos = bullet.pos;
        
        // Update position
        bullet.pos += bullet.velocity * deltaTime;
        bullet.lifetime -= deltaTime;

        // Deactivate if lifetime expired
        if (bullet.lifetime <= 0) {
            bullet.active = false;
            continue;
        }

        // Check collision with boss AABB
        if (bullet.pos.x >= bossMin.x && bullet.pos.x <= bossMax.x &&
            bullet.pos.y >= bossMin.y && bullet.pos.y <= bossMax.y) {
            
            // Hit boss!
            damageBoss(bullet.damage, bullet.cid);
            bullet.active = false; // Destroy bullet
            // std::cout << "[BossFight] Bullet hit boss!" << std::endl;
        }
    }

    // Remove inactive bullets
    playerBullets.erase(
        std::remove_if(playerBullets.begin(), playerBullets.end(),
            [](const PlayerBullet& b) { return !b.active; }),
        playerBullets.end()
    );
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

// ============================================================================
// Helper Methods
// ============================================================================

glm::vec2 BossFightManager::getRandomSpawnPosition() {
    // Use map-relative positions if map data is available
    if (currentMap && currentMap->data) {
        float mapW = (float)currentMap->w;
        float mapH = (float)currentMap->h;
        
        // Calculate spawn positions relative to map size (with 10% margin from edges)
        float margin = 0.1f;
        glm::vec2 spawnPositions[] = {
            {mapW * 0.5f, mapH * margin + 3.0f},      // North (center-top)
            {mapW * 0.5f, mapH * (1.0f - margin) - 3.0f}, // South (center-bottom)
            {mapW * margin + 3.0f, mapH * 0.5f},      // West (left-center)
            {mapW * (1.0f - margin) - 3.0f, mapH * 0.5f}  // East (right-center)
        };
        
        std::uniform_int_distribution<int> dist(0, 3);
        glm::vec2 pos = spawnPositions[dist(rng)];
        
        // Clamp to valid map bounds (leaving room for 5x5 boss size)
        pos.x = std::max(5.0f, std::min(pos.x, mapW - 5.0f));
        pos.y = std::max(5.0f, std::min(pos.y, mapH - 5.0f));
        
        return pos;
    }
    
    // Fallback for no map data - use center position
    return glm::vec2(30.0f, 30.0f);
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

int BossFightManager::getPlayerDamageDealt(int32_t cid) const {
    auto it = playerDamageDealt.find(cid);
    if (it != playerDamageDealt.end()) {
        return it->second;
    }
    return 0;
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
    
    // Check 5x5 area around center for boss hitbox
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int checkX = x + dx;
            int checkY = y + dy;
            
            if (checkX < 0 || checkY < 0 || checkX >= mapData->w || checkY >= mapData->h) {
                return false;
            }
            
            phisics::BlockInfo& block = mapData->get(checkX, checkY);
            if (block.isCollidable()) {
                return false;
            }
        }
    }
    
    return true;
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
    
    // Log removed for performance
    // std::cout << "[BossFight] Moving boss..." << std::endl;
    
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
        
        // std::cout << "[BossFight] Following waypoint " << bossPathIndex << "/" << bossPath.size() 
        //           << " at (" << waypoint.x << ", " << waypoint.y << "), distance: " << distance << std::endl;
        
        if (distance < 0.3f) {
            // Reached waypoint, move to next
            bossPathIndex++;
            // std::cout << "[BossFight] Reached waypoint, moving to next" << std::endl;
        } else {
            // Move towards waypoint
            direction = glm::normalize(direction);
            boss.velocity = direction * boss.speed;
            boss.position += boss.velocity * deltaTime;
        }
    } else {
        // Fallback: simple movement towards target if pathfinding fails
        // std::cout << "[BossFight] Path empty/finished, using direct movement fallback" << std::endl;
        glm::vec2 direction = target.pos - boss.position;
        float distance = glm::length(direction);
        
        if (distance > 0.1f) {
            direction = glm::normalize(direction);
            boss.velocity = direction * boss.speed;
            boss.position += boss.velocity * deltaTime;
        }
    }
    
    // Resolve collisions with map
    if (mapData) {
        resolveBossCollision(mapData);
    } else {
        std::cout << "[BossFight] No valid path or reached end" << std::endl;
    }
}

void BossFightManager::resolveBossCollision(phisics::MapData* mapData) {
    if (!mapData || !mapData->data) return;
    
    // Boss is 5x5 tiles with center-based positioning
    float halfSize = BOSS_HITBOX_HALF;
    
    // Clamp to map bounds
    if (boss.position.x - halfSize < 0) boss.position.x = halfSize;
    if (boss.position.y - halfSize < 0) boss.position.y = halfSize;
    if (boss.position.x + halfSize > mapData->w) boss.position.x = mapData->w - halfSize;
    if (boss.position.y + halfSize > mapData->h) boss.position.y = mapData->h - halfSize;
    
    // Check collision with map tiles in 5x5 area
    int minX = (int)(boss.position.x - halfSize);
    int maxX = (int)(boss.position.x + halfSize);
    int minY = (int)(boss.position.y - halfSize);
    int maxY = (int)(boss.position.y + halfSize);
    
    // Simple collision resolution - push back if colliding
    for (int y = minY; y <= maxY && y < mapData->h; y++) {
        for (int x = minX; x <= maxX && x < mapData->w; x++) {
            if (x < 0 || y < 0) continue;
            
            phisics::BlockInfo& block = mapData->get(x, y);
            if (block.isCollidable()) {
                // Push boss away from wall
                glm::vec2 blockCenter(x + 0.5f, y + 0.5f);
                glm::vec2 pushDir = boss.position - blockCenter;
                
                if (glm::length(pushDir) > 0.01f) {
                    pushDir = glm::normalize(pushDir);
                    boss.position += pushDir * 0.1f;
                }
            }
        }
    }
}

void BossFightManager::checkBossPlayerCollision(std::map<int32_t, phisics::Entity>& players, float deltaTime) {
    // Boss hitbox: 5x5 tiles, center-based
    glm::vec2 bossMin = boss.position - glm::vec2(BOSS_HITBOX_HALF, BOSS_HITBOX_HALF);
    glm::vec2 bossMax = boss.position + glm::vec2(BOSS_HITBOX_HALF, BOSS_HITBOX_HALF);
    
    for (auto& pair : players) {
        if (!isPlayerAlive(pair.first)) continue;
        
        phisics::Entity& player = pair.second;
        glm::vec2 playerMin = player.pos;
        glm::vec2 playerMax = player.pos + player.dimensions;
        
        // AABB collision check
        if (bossMax.x > playerMin.x && bossMin.x < playerMax.x &&
            bossMax.y > playerMin.y && bossMin.y < playerMax.y) {
            
            // Collision detected - push player away and deal damage
            glm::vec2 playerCenter = player.pos + player.dimensions * 0.5f;
            glm::vec2 pushDir = playerCenter - boss.position;
            
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
        
        for (auto& pair : players) {
            if (!isPlayerAlive(pair.first)) continue;
            
            phisics::Entity& player = pair.second;
            glm::vec2 playerCenter = player.pos + player.dimensions * 0.5f;
            float distance = glm::length(playerCenter - boss.position);
            
            // Check if player is within damage radius (6-7 tiles)
            if (distance <= proximityDamageRadius) {
                applyDamageToPlayer(pair.first, player, proximityDamageAmount, 
                                   BossAttackType::MELEE, glm::vec2(0, 0));
                
                std::cout << "[BossFight] Proximity damage: " << proximityDamageAmount 
                         << " to player " << pair.first 
                         << " (distance: " << distance << ")" << std::endl;
            }
        }
    }
}

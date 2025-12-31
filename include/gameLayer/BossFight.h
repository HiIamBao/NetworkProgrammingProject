#pragma once

#include <glm/vec2.hpp>
#include <cstdint>
#include <common/BaseEntity.h>

// ============================================================================
// BOSS FIGHT MODE - DATA STRUCTURES & ENUMS
// ============================================================================
// Players cooperatively fight a powerful boss with unique abilities.
// Boss has multiple phases, special attacks, and can summon minions.
// ============================================================================

namespace BossFight {

// ============================================================================
// GAME STATE ENUMS
// ============================================================================

enum class BossFightState {
    WAITING = 0,            // Waiting for match to start
    BOSS_SPAWNING = 1,      // Boss spawn animation/phase
    BOSS_ACTIVE = 2,        // Boss is alive and fighting
    BOSS_DEFEATED = 3,      // Boss defeated, victory
    PLAYERS_DEFEATED = 4    // All players dead, defeat
};

// ============================================================================
// BOSS TYPES
// ============================================================================

enum class BossType {
    GIANT_DEMON = 0    // First boss implementation
};

// ============================================================================
// BOSS ATTACK TYPES
// ============================================================================

enum class BossAttackType {
    MELEE = 0,         // Basic melee attack (30 damage, single target)
    AOE_SLAM = 1,      // Ground slam (100 damage, 3-tile radius)
    CHARGE = 2,        // Charges at target (50 damage, knockback)
    SUMMON_MINIONS = 3 // Spawns 3-5 small enemies
};

// ============================================================================
// BOSS PHASE
// ============================================================================

enum class BossPhase {
    PHASE_1 = 0,  // 100%-70% HP: Basic attacks only
    PHASE_2 = 1,  // 70%-40% HP: AOE attacks + faster
    PHASE_3 = 2   // 40%-0% HP: All attacks + minions
};

// ============================================================================
// BOSS DATA STRUCTURE
// ============================================================================



// ...

struct Boss : public phisics::BaseEntity {
    int32_t bossId;
    BossType type;
    BossPhase currentPhase;
    
    // pos, velocity, health, maxHealth, speed, isAlive inherited
    
    int baseDamage;
    int32_t currentTargetId;  // CID of targeted player
    float lastAttackTime;     // For attack cooldown
    float nextAttackTimer;    // Time until next attack
    BossAttackType nextAttackType; // Queued attack
    
    Boss() {
         bossId = 1;
         type = BossType::GIANT_DEMON;
         currentPhase = BossPhase::PHASE_1;
         pos = glm::vec2(0, 0);
         velocity = glm::vec2(0, 0);
         health = 5000;
         maxHealth = 5000;
         speed = 4.0f;
         baseDamage = 30;
         currentTargetId = -1;
         isAlive = true;
         lastAttackTime = 0.0f;
         nextAttackTimer = 2.0f;
         nextAttackType = BossAttackType::MELEE;
    }
};

// ============================================================================
// MINION DATA STRUCTURE
// ============================================================================

struct Minion : public phisics::BaseEntity {
    int32_t minionId;
    // pos, velocity, health, maxHealth, speed, isAlive inherited
    
    int damage;
    int32_t targetPlayerId;
    float lastAttackTime;
    
    Minion() {
        minionId = 0;
        pos = glm::vec2(0, 0);
        velocity = glm::vec2(0, 0);
        health = 50;
        maxHealth = 50;
        speed = 6.0f;
        damage = 10;
        targetPlayerId = -1;
        isAlive = true;
        lastAttackTime = 0.0f;
    }
};

// ============================================================================
// BOSS STATS CONFIGURATION
// ============================================================================

struct BossStats {
    BossType type;
    float baseHealth;
    float baseSpeed;
    int baseDamage;
    float attackCooldown;
    
    static BossStats getStats(BossType type) {
        BossStats stats;
        stats.type = type;
        
        switch (type) {
            case BossType::GIANT_DEMON:
                stats.baseHealth = 5000.0f;
                stats.baseSpeed = 4.0f;
                stats.baseDamage = 30;
                stats.attackCooldown = 2.5f;
                break;
        }
        return stats;
    }
};

// ============================================================================
// GAME CONSTANTS
// ============================================================================

constexpr float BOSS_SPAWN_DURATION = 3.0f;   // Spawn animation time
constexpr float AOE_SLAM_RADIUS = 3.0f;       // 3 tiles radius
constexpr int AOE_SLAM_DAMAGE = 100;
constexpr float BOSS_ATTACK_RANGE = 2.0f;     // Melee attack range
constexpr float CHARGE_RANGE = 15.0f;         // Max charge distance
constexpr int CHARGE_DAMAGE = 50;
constexpr int MINION_SPAWN_COUNT = 4;         // Minions per summon
constexpr float MINION_ATTACK_RANGE = 1.5f;   // Minion melee range
constexpr float MINION_ATTACK_COOLDOWN = 1.0f; // Minion attack cooldown

// Arena spawn positions (for 60x60 map)
constexpr float BOSS_SPAWN_X = 30.0f;
constexpr float BOSS_SPAWN_Y = 30.0f;

} // namespace BossFight

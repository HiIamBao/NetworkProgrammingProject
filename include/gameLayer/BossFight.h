#pragma once

#include <glm/vec2.hpp>
#include <cstdint>

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
    CIRCLE_SPRAY = 1   // Circle bullet spray (360° attack, 8-12 bullets)
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

struct Boss {
    int32_t bossId;
    BossType type;
    BossPhase currentPhase;
    glm::vec2 position;
    glm::vec2 velocity;
    float health;
    float maxHealth;
    float speed;
    int baseDamage;
    int32_t currentTargetId;  // CID of targeted player
    bool isAlive;
    float lastAttackTime;     // For melee attack cooldown
    float nextAttackTimer;    // Time until next attack
    BossAttackType nextAttackType; // Queued attack
    float lastSkillTime;      // Circle spray attack cooldown tracker
    float skillCooldown;      // Configurable skill timing (phase-dependent)
    bool isSprayingCircle;    // Active circle spray state
    
    Boss() : bossId(1), type(BossType::GIANT_DEMON), currentPhase(BossPhase::PHASE_1),
             position(0, 0), velocity(0, 0), health(5000), maxHealth(5000),
             speed(4.0f), baseDamage(30), currentTargetId(-1), isAlive(true),
             lastAttackTime(0.0f), nextAttackTimer(2.5f), 
             nextAttackType(BossAttackType::MELEE), lastSkillTime(0.0f),
             skillCooldown(3.0f), isSprayingCircle(false) {}
};

// ============================================================================
// CIRCLE SPRAY ATTACK DATA
// ============================================================================

struct CircleShootAttack {
    glm::vec2 centerPos;      // Center position of spray
    int bulletCount;          // 8-12 bullets in circle
    float bulletSpeed;        // Speed of bullets
    int damage;               // Damage per bullet
    
    CircleShootAttack() : centerPos(0, 0), bulletCount(12), 
                          bulletSpeed(8.0f), damage(30) {}
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
constexpr float BOSS_SIZE = 5.0f;             // Boss is 5x5 tiles
constexpr float BOSS_HITBOX_HALF = 2.5f;      // Half size for calculations
constexpr float BOSS_ATTACK_RANGE = 2.5f;     // Melee attack range
constexpr float BOSS_CONTACT_RADIUS = 7.0f;   // Contact damage radius (6-7 tiles)
constexpr int BOSS_CONTACT_DAMAGE_PHASE1 = 5;
constexpr int BOSS_CONTACT_DAMAGE_PHASE2 = 10;
constexpr int BOSS_CONTACT_DAMAGE_PHASE3 = 15;

// Circle spray attack constants
constexpr int CIRCLE_SPRAY_BULLETS = 12;       // Bullets per spray
constexpr float CIRCLE_SPRAY_SPEED = 8.0f;     // Bullet speed
constexpr float CIRCLE_SPRAY_COOLDOWN_PHASE1 = 3.0f;  // Never used in Phase 1
constexpr float CIRCLE_SPRAY_COOLDOWN_PHASE2 = 3.0f;
constexpr float CIRCLE_SPRAY_COOLDOWN_PHASE3 = 1.5f;

// Phase transition thresholds
constexpr float PHASE_2_THRESHOLD = 0.7f;      // 70% HP
constexpr float PHASE_3_THRESHOLD = 0.4f;      // 40% HP

// Phase speed multipliers
constexpr float PHASE_1_SPEED = 4.0f;
constexpr float PHASE_2_SPEED_MULT = 1.2f;     // 4.8 tiles/s
constexpr float PHASE_3_SPEED_MULT = 1.15f;    // 5.52 tiles/s (cumulative 1.38x)

// Arena spawn positions (for 60x60 map)
constexpr float BOSS_SPAWN_X = 30.0f;
constexpr float BOSS_SPAWN_Y = 30.0f;

} // namespace BossFight

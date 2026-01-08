#pragma once

#include <glm/vec2.hpp>
#include <cstdint>
#include <algorithm>

// ============================================================================
// HORDE DEFENSE MODE - DATA STRUCTURES & ENUMS
// ============================================================================
// This file contains all data structures and enums for the Horde Defense game mode.
// Players fight waves of AI enemies and upgrade their stats through a shop system.
// NO weapon switching - players use basic shooting with stat upgrades.
// ============================================================================

namespace HordeDefense {

// ============================================================================
// GAME STATE ENUMS
// ============================================================================

enum class HordeDefenseState {
    WAITING = 0,            // Waiting for match to start
    BUYING_PHASE = 1,       // Players can buy upgrades/items (30s between waves)
    WAVE_ACTIVE = 2,        // Wave in progress, enemies spawning/fighting
    WAVE_COMPLETE = 3,      // Wave finished, transitioning to buy phase
    VICTORY = 4,            // All waves completed successfully
    DEFEAT = 5              // All players died
};

// ============================================================================
// ENEMY TYPES
// ============================================================================

enum class EnemyType {
    ZOMBIE = 0,     // Slow, low HP, basic melee
    RUNNER = 1,     // Fast, medium HP, charges at players
    TANK = 2,       // Very slow, very high HP, heavy damage
    EXPLODER = 3,   // Medium speed, explodes on death/contact
    ELITE = 4,      // (Old Boss) Tough normal mob, 500 HP
    BOSS_WAVE5 = 5, // Summoner Boss
    BOSS_WAVE10 = 6,// Bullet Hell Boss
    BOSS_WAVE15 = 7,// Explosive Boss
    BOSS_WAVE20 = 8 // Final Boss
};

// ============================================================================
// UPGRADE TYPES (Permanent stat increases)
// ============================================================================

enum class UpgradeType {
    DAMAGE = 0,         // Increase bullet damage
    FIRE_RATE = 1,      // Increase shooting speed
    HEALTH = 2,         // Increase max health
    SPEED = 3,          // Increase movement speed
    BULLET_SPEED = 4    // Increase bullet velocity
};

// Total number of upgrade types
constexpr int UPGRADE_COUNT = 5;

// ============================================================================
// SHOP ITEM TYPES (Consumables & temporary buffs)
// ============================================================================

enum class ShopItemType {
    HEALTH_PACK = 0,        // Restore 1 HP instantly
    SPEED_BOOST = 1,        // +50% move speed for 1 wave
    DAMAGE_AMPLIFIER = 2,   // +100% damage for 1 wave
    MULTI_SHOT = 3          // Shoot 3 bullets at once for 1 wave
};

// Total number of shop item types
constexpr int SHOP_ITEM_COUNT = 4;

// ============================================================================
// ENEMY DATA STRUCTURE
// ============================================================================

struct EnemyStats {
    EnemyType type;
    float baseHealth;
    float baseSpeed;
    int baseDamage;
    int moneyReward;
    
    // Get stats for enemy type
    static EnemyStats getStats(EnemyType type) {
        EnemyStats stats;
        stats.type = type;
        
        switch (type) {
            case EnemyType::ZOMBIE:
                stats.baseHealth = 40.0f;
                stats.baseSpeed = 1.0f;
                stats.baseDamage = 2;
                stats.moneyReward = 10;
                break;
            case EnemyType::RUNNER:
                stats.baseHealth = 30.0f;
                stats.baseSpeed = 2.5f;
                stats.baseDamage = 2;
                stats.moneyReward = 15;
                break;
            case EnemyType::TANK:
                stats.baseHealth = 200.0f;
                stats.baseSpeed = 0.6f;
                stats.baseDamage = 4;
                stats.moneyReward = 50;
                break;
            case EnemyType::EXPLODER:
                stats.baseHealth = 40.0f;
                stats.baseSpeed = 1.2f;
                stats.baseDamage = 10;
                stats.moneyReward = 25;
                break;
            case EnemyType::ELITE:
                stats.baseHealth = 500.0f;
                stats.baseSpeed = 0.8f;
                stats.baseDamage = 5;
                stats.moneyReward = 150;
                break;
            case EnemyType::BOSS_WAVE5:
                stats.baseHealth = 2000.0f;
                stats.baseSpeed = 0.5f;
                stats.baseDamage = 5;
                stats.moneyReward = 1000;
                break;
            case EnemyType::BOSS_WAVE10:
                stats.baseHealth = 3500.0f;
                stats.baseSpeed = 0.6f;
                stats.baseDamage = 5;
                stats.moneyReward = 2000;
                break;
            case EnemyType::BOSS_WAVE15:
                stats.baseHealth = 5000.0f;
                stats.baseSpeed = 0.7f;
                stats.baseDamage = 8;
                stats.moneyReward = 3000;
                break;
            case EnemyType::BOSS_WAVE20:
                stats.baseHealth = 10000.0f;
                stats.baseSpeed = 0.8f;
                stats.baseDamage = 10;
                stats.moneyReward = 5000;
                break;
        }
        
        return stats;
    }
};

struct Enemy {
    int32_t id;                 // Unique enemy ID
    EnemyType type;             // Enemy type
    glm::vec2 position;         // Current position
    glm::vec2 velocity;         // Movement velocity
    float health;               // Current health
    float maxHealth;            // Maximum health
    float speed;                // Movement speed multiplier
    int damage;                 // Damage dealt to players
    int32_t targetPlayerId;     // Current target player CID
    bool isAlive;               // Alive status
    float lastAttackTime;       // Time of last attack (for cooldown)
    
    Enemy() : id(0), type(EnemyType::ZOMBIE), position(0, 0), velocity(0, 0),
              health(0), maxHealth(0), speed(1.0f), damage(0), 
              targetPlayerId(-1), isAlive(true), lastAttackTime(0.0f) {}
};

// ============================================================================
// UPGRADE SYSTEM
// ============================================================================

struct UpgradeInfo {
    UpgradeType type;
    int baseCost;           // Cost for level 1
    float effectPerLevel;   // Effect multiplier per level
    int maxLevel;           // Maximum upgrade level
    const char* name;
    const char* description;
    
    static UpgradeInfo getInfo(UpgradeType type) {
        UpgradeInfo info;
        info.type = type;
        info.maxLevel = 5;  // All upgrades have 5 levels
        
        switch (type) {
            case UpgradeType::DAMAGE:
                info.baseCost = 200;
                info.effectPerLevel = 0.25f;  // +25% per level
                info.name = "Damage Upgrade";
                info.description = "+25% bullet damage per level";
                break;
            case UpgradeType::FIRE_RATE:
                info.baseCost = 250;
                info.effectPerLevel = 0.20f;  // +20% per level
                info.name = "Fire Rate Upgrade";
                info.description = "+20% faster shooting per level";
                break;
            case UpgradeType::HEALTH:
                info.baseCost = 150;
                info.effectPerLevel = 1.0f;  // +1 HP per level
                info.name = "Health Upgrade";
                info.description = "+1 max HP per level";
                break;
            case UpgradeType::SPEED:
                info.baseCost = 200;
                info.effectPerLevel = 0.15f;  // +15% per level
                info.name = "Speed Upgrade";
                info.description = "+15% movement speed per level";
                break;
            case UpgradeType::BULLET_SPEED:
                info.baseCost = 180;
                info.effectPerLevel = 0.30f;  // +30% per level
                info.name = "Bullet Speed Upgrade";
                info.description = "+30% bullet velocity per level";
                break;
        }
        
        return info;
    }
    
    // Calculate cost for a specific level
    int getCostForLevel(int level) const {
        if (level < 1 || level > maxLevel) return 0;
        // Cost increases per level: level 1 = base, level 2 = base*1.5, level 3 = base*2, etc.
        return baseCost + (level - 1) * (baseCost / 2);
    }
};

struct PlayerUpgrades {
    int damageLevel;        // 0-5
    int fireRateLevel;      // 0-5
    int healthLevel;        // 0-5
    int speedLevel;         // 0-5
    int bulletSpeedLevel;   // 0-5
    
    PlayerUpgrades() : damageLevel(0), fireRateLevel(0), healthLevel(0),
                      speedLevel(0), bulletSpeedLevel(0) {}
    
    int getLevel(UpgradeType type) const {
        switch (type) {
            case UpgradeType::DAMAGE: return damageLevel;
            case UpgradeType::FIRE_RATE: return fireRateLevel;
            case UpgradeType::HEALTH: return healthLevel;
            case UpgradeType::SPEED: return speedLevel;
            case UpgradeType::BULLET_SPEED: return bulletSpeedLevel;
        }
        return 0;
    }
    
    void setLevel(UpgradeType type, int level) {
        switch (type) {
            case UpgradeType::DAMAGE: damageLevel = level; break;
            case UpgradeType::FIRE_RATE: fireRateLevel = level; break;
            case UpgradeType::HEALTH: healthLevel = level; break;
            case UpgradeType::SPEED: speedLevel = level; break;
            case UpgradeType::BULLET_SPEED: bulletSpeedLevel = level; break;
        }
    }
};

// ============================================================================
// SHOP ITEMS
// ============================================================================

struct ShopItemInfo {
    ShopItemType type;
    int cost;
    float effectValue;      // Effect amount (HP restored, damage %, etc.)
    float duration;         // Duration in waves (0 = instant, 1 = one wave, etc.)
    const char* name;
    const char* description;
    
    static ShopItemInfo getInfo(ShopItemType type) {
        ShopItemInfo info;
        info.type = type;
        
        switch (type) {
            case ShopItemType::HEALTH_PACK:
                info.cost = 50;
                info.effectValue = 1.0f;    // Restore 1 HP
                info.duration = 0.0f;       // Instant
                info.name = "Health Pack";
                info.description = "Restore 1 HP instantly";
                break;
            case ShopItemType::SPEED_BOOST:
                info.cost = 150;
                info.effectValue = 0.5f;   // +50% speed
                info.duration = 1.0f;      // 1 wave
                info.name = "Speed Boost";
                info.description = "+50% movement speed for 1 wave";
                break;
            case ShopItemType::DAMAGE_AMPLIFIER:
                info.cost = 200;
                info.effectValue = 1.0f;   // +100% damage
                info.duration = 1.0f;      // 1 wave
                info.name = "Damage Amplifier";
                info.description = "+100% damage for 1 wave";
                break;
            case ShopItemType::MULTI_SHOT:
                info.cost = 250;
                info.effectValue = 3.0f;   // 3 bullets at once
                info.duration = 1.0f;      // 1 wave
                info.name = "Multi-Shot";
                info.description = "Shoot 3 bullets at once for 1 wave";
                break;
        }
        
        return info;
    }
};

struct ActiveBuff {
    ShopItemType type;
    float remainingTime;    // Time remaining (-1 = permanent until death, 0 = expired)
    float effectValue;      // Current effect value
    
    ActiveBuff() : type(ShopItemType::HEALTH_PACK), remainingTime(0), effectValue(0) {}
    ActiveBuff(ShopItemType t, float time, float value) 
        : type(t), remainingTime(time), effectValue(value) {}
    
    bool isActive() const { return remainingTime != 0; }
    void update(float deltaTime) {
        if (remainingTime > 0) {
            remainingTime -= deltaTime;
            if (remainingTime <= 0) remainingTime = 0;
        }
    }
};

// ============================================================================
// WAVE CONFIGURATION
// ============================================================================

struct WaveConfig {
    int waveNumber;
    int zombieCount;
    int runnerCount;
    int tankCount;
    int exploderCount;
    int eliteCount;
    EnemyType bossType;
    bool hasBoss;
    float spawnInterval;    // Seconds between spawns
    int completionBonus;    // Money awarded on wave complete
    
    static WaveConfig getWaveConfig(int wave) {
        WaveConfig config;
        config.waveNumber = wave;
        config.completionBonus = 200 * wave; // Higher bonus for faster scaling
        config.hasBoss = false;
        config.bossType = EnemyType::ZOMBIE; // Default
        
        // Spawn interval decreases (faster spawning)
        config.spawnInterval = std::max(0.3f, 1.5f - (wave * 0.05f));
        
        // Config for ~5-10 min game (quick pacing)
        config.zombieCount = 8 + (wave * 3);
        config.runnerCount = wave >= 2 ? (wave * 2) : 0;
        config.tankCount = wave >= 4 ? (wave / 2) : 0;
        config.exploderCount = wave >= 6 ? (wave / 2) : 0;
        config.eliteCount = wave >= 8 ? (wave / 4) : 0; // Elites start wave 8
        
        // Boss Waves
        if (wave == 5) {
            config.hasBoss = true;
            config.bossType = EnemyType::BOSS_WAVE5;
            config.zombieCount = 10; // Minions
        } else if (wave == 10) {
            config.hasBoss = true;
            config.bossType = EnemyType::BOSS_WAVE10;
            config.zombieCount = 15;
            config.eliteCount = 2;
        } else if (wave == 15) {
            config.hasBoss = true;
            config.bossType = EnemyType::BOSS_WAVE15;
            config.tankCount = 5;
            config.eliteCount = 4;
        } else if (wave == 20) {
            config.hasBoss = true;
            config.bossType = EnemyType::BOSS_WAVE20;
            config.zombieCount = 20;
            config.tankCount = 10;
            config.eliteCount = 8;
        }
        
        return config;
    }
    
    int getTotalEnemies() const {
        return zombieCount + runnerCount + tankCount + exploderCount + eliteCount + (hasBoss ? 1 : 0);
    }
};

// ============================================================================
// GAME CONSTANTS
// ============================================================================

constexpr int TOTAL_WAVES = 20;
constexpr int STARTING_MONEY = 500;
constexpr float BUY_PHASE_DURATION = 30.0f;  // 30 seconds
constexpr float ENEMY_ATTACK_COOLDOWN = 1.0f; // 1 second between attacks
constexpr float ENEMY_ATTACK_RANGE = 1.5f;    // Attack range in tiles

} // namespace HordeDefense

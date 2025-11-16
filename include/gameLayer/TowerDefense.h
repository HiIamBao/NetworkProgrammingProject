#pragma once
#include <glm/vec2.hpp>
#include <cstdint>
#include <vector>

// ============================================================================
// ENUMS
// ============================================================================

enum class TowerType {
    ARROW = 0,      // Fast attack, low damage
    CANNON = 1,     // Slow attack, high damage, splash
    MAGIC = 2,      // Medium attack, chain lightning
    SLOW = 3        // Fast attack, low damage, slows enemies
};

enum class EnemyType {
    WEAK = 0,       // Low health, normal speed, low reward
    NORMAL = 1,     // Medium health, normal speed, medium reward
    FAST = 2,       // Low health, high speed, medium reward
    TANK = 3,       // High health, slow speed, high reward
    BOSS = 4        // Very high health, slow speed, very high reward
};

enum class TowerDefenseState {
    WAITING = 0,        // Waiting for players to ready up
    BUILDING = 1,       // Build phase between waves
    WAVE_ACTIVE = 2,    // Wave in progress
    WAVE_COMPLETE = 3,  // Wave finished, transitioning
    VICTORY = 4,        // All waves completed
    DEFEAT = 5          // Base destroyed
};

// ============================================================================
// TOWER DATA STRUCTURES
// ============================================================================

struct TowerStats {
    int cost;           // Build cost
    int damage;         // Damage per shot
    float range;        // Attack range in tiles
    float fireRate;     // Attacks per second
    int upgradeCost;    // Cost to upgrade to next level
    
    // Special properties
    bool hasSplash;     // Splash damage
    float splashRadius; // Splash damage radius
    bool hasChain;      // Chain lightning effect
    int chainTargets;   // Number of chain targets
    bool hasSlow;       // Slowing effect
    float slowAmount;   // Slow percentage (0.0 - 1.0)
    float slowDuration; // Slow duration in seconds
};

struct Tower {
    int32_t towerId;           // Unique tower ID
    int32_t ownerId;           // Player CID who built it
    TowerType type;            // Tower type
    glm::vec2 position;        // Position on map (tile coordinates)
    int level;                 // Tower level (1-3)
    float timeSinceLastShot;   // Time accumulator for fire rate
    int32_t currentTargetId;   // Current enemy being targeted (-1 if none)
    
    // Calculated stats (based on type and level)
    TowerStats stats;
    
    Tower() : towerId(-1), ownerId(-1), type(TowerType::ARROW), 
              position(0, 0), level(1), timeSinceLastShot(0.0f), 
              currentTargetId(-1) {}
};

// ============================================================================
// ENEMY DATA STRUCTURES
// ============================================================================

struct EnemyStats {
    int maxHealth;      // Maximum health
    float speed;        // Movement speed (tiles per second)
    int moneyReward;    // Money awarded on death
    int baseDamage;     // Damage dealt to base
};

struct Enemy {
    int32_t enemyId;           // Unique enemy ID
    EnemyType type;            // Enemy type
    glm::vec2 position;        // Current position
    glm::vec2 lastPosition;    // Previous position (for interpolation)
    int currentHealth;         // Current health
    int maxHealth;             // Maximum health
    int currentWaypointIndex;  // Current waypoint in path
    float slowMultiplier;      // Speed multiplier from slow effects (1.0 = normal)
    float slowTimer;           // Remaining slow duration
    bool reachedBase;          // Has this enemy reached the base?
    
    // Calculated stats
    EnemyStats stats;
    
    Enemy() : enemyId(-1), type(EnemyType::NORMAL), position(0, 0), 
              lastPosition(0, 0), currentHealth(0), maxHealth(0), 
              currentWaypointIndex(0), slowMultiplier(1.0f), 
              slowTimer(0.0f), reachedBase(false) {}
};

// ============================================================================
// WAVE DATA STRUCTURES
// ============================================================================

struct WaveComposition {
    int weakCount;
    int normalCount;
    int fastCount;
    int tankCount;
    int bossCount;
};

struct Wave {
    int waveNumber;             // Wave number (1-based)
    WaveComposition enemies;    // Enemy composition
    float spawnInterval;        // Time between enemy spawns (seconds)
    int moneyBonus;            // Bonus money for completing wave
    int earlyStartBonus;       // Bonus money for starting wave early
};

// ============================================================================
// BASE DATA STRUCTURE
// ============================================================================

struct Base {
    glm::vec2 position;     // Base position on map
    int health;             // Current health
    int maxHealth;          // Maximum health
    float radius;           // Visual radius for rendering
    
    Base() : position(0, 0), health(100), maxHealth(100), radius(2.0f) {}
};

// ============================================================================
// PATH DATA STRUCTURE
// ============================================================================

struct PathWaypoint {
    glm::vec2 position;     // Waypoint position
};

struct EnemyPath {
    std::vector<PathWaypoint> waypoints;
    glm::vec2 spawnPoint;       // Enemy spawn location
    glm::vec2 basePoint;        // Base/goal location
};

// ============================================================================
// GAME STATE DATA STRUCTURE
// ============================================================================

struct TowerDefenseGameState {
    TowerDefenseState state;        // Current game state
    int currentWave;                // Current wave number (0-based, 0 = not started)
    int totalWaves;                 // Total number of waves
    float waveTimer;                // Build timer or wave timer
    float waveBuildTime;            // Time between waves for building
    int baseHealth;                 // Current base health
    int baseMaxHealth;              // Maximum base health
    int enemiesRemaining;           // Enemies left in current wave
    int enemiesSpawned;             // Total enemies spawned in current wave
    
    TowerDefenseGameState() : state(TowerDefenseState::WAITING), 
                              currentWave(0), totalWaves(20), 
                              waveTimer(30.0f), waveBuildTime(30.0f),
                              baseHealth(100), baseMaxHealth(100),
                              enemiesRemaining(0), enemiesSpawned(0) {}
};

// ============================================================================
// PLAYER TOWER DEFENSE DATA
// ============================================================================

struct PlayerTowerDefenseData {
    int32_t cid;                // Player CID
    int money;                  // Current money
    int enemiesKilled;          // Total enemies killed by this player's towers
    int towersBuilt;            // Total towers built
    int damageDealt;            // Total damage dealt
    
    PlayerTowerDefenseData() : cid(-1), money(500), enemiesKilled(0), 
                               towersBuilt(0), damageDealt(0) {}
};

// ============================================================================
// TOWER ATTACK DATA (for visual effects)
// ============================================================================

struct TowerAttackVisual {
    int32_t towerId;            // Tower that fired
    int32_t targetEnemyId;      // Target enemy
    glm::vec2 startPos;         // Attack start position
    glm::vec2 endPos;           // Attack end position
    TowerType towerType;        // Type for visual effect selection
    
    TowerAttackVisual() : towerId(-1), targetEnemyId(-1), 
                         startPos(0, 0), endPos(0, 0), 
                         towerType(TowerType::ARROW) {}
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Get default stats for tower type at level 1
TowerStats GetTowerStats(TowerType type, int level = 1);

// Get default stats for enemy type
EnemyStats GetEnemyStats(EnemyType type);

// Generate wave composition for given wave number
WaveComposition GenerateWaveComposition(int waveNumber);

// Calculate upgrade cost for tower
int GetTowerUpgradeCost(TowerType type, int currentLevel);

// Get tower sell value (50% of total investment)
int GetTowerSellValue(TowerType type, int currentLevel);

#include "TowerDefense.h"
#include <cmath>

// ============================================================================
// TOWER STATS IMPLEMENTATION
// ============================================================================

TowerStats GetTowerStats(TowerType type, int level) {
    TowerStats stats = {};
    
    // Base stats multiplier per level
    float levelMultiplier = 1.0f + (level - 1) * 0.2f;  // +20% per level
    float rangeBonus = (level - 1) * 0.5f;  // +0.5 tiles per level
    float fireRateMultiplier = 1.0f + (level - 1) * 0.1f;  // +10% per level
    
    switch (type) {
        case TowerType::ARROW:
            stats.cost = 100;
            stats.damage = static_cast<int>(10 * levelMultiplier);
            stats.range = 5.0f + rangeBonus;
            stats.fireRate = 1.0f * fireRateMultiplier;  // 1 attack per second
            stats.upgradeCost = (level < 3) ? 50 + (level - 1) * 25 : 0;
            stats.hasSplash = false;
            stats.hasChain = false;
            stats.hasSlow = false;
            break;
            
        case TowerType::CANNON:
            stats.cost = 200;
            stats.damage = static_cast<int>(40 * levelMultiplier);
            stats.range = 6.0f + rangeBonus;
            stats.fireRate = 0.4f * fireRateMultiplier;  // 0.4 attacks per second (2.5s cooldown)
            stats.upgradeCost = (level < 3) ? 100 + (level - 1) * 50 : 0;
            stats.hasSplash = true;
            stats.splashRadius = 1.5f + (level - 1) * 0.3f;
            stats.hasChain = false;
            stats.hasSlow = false;
            break;
            
        case TowerType::MAGIC:
            stats.cost = 250;
            stats.damage = static_cast<int>(25 * levelMultiplier);
            stats.range = 7.0f + rangeBonus;
            stats.fireRate = 0.67f * fireRateMultiplier;  // 0.67 attacks per second (1.5s cooldown)
            stats.upgradeCost = (level < 3) ? 125 + (level - 1) * 60 : 0;
            stats.hasSplash = false;
            stats.hasChain = true;
            stats.chainTargets = 2 + (level - 1);  // 2, 3, 4 targets
            stats.hasSlow = false;
            break;
            
        case TowerType::SLOW:
            stats.cost = 150;
            stats.damage = static_cast<int>(5 * levelMultiplier);
            stats.range = 4.0f + rangeBonus;
            stats.fireRate = 1.0f * fireRateMultiplier;  // 1 attack per second
            stats.upgradeCost = (level < 3) ? 75 + (level - 1) * 35 : 0;
            stats.hasSplash = false;
            stats.hasChain = false;
            stats.hasSlow = true;
            stats.slowAmount = 0.5f + (level - 1) * 0.1f;  // 50%, 60%, 70% slow
            stats.slowDuration = 2.0f + (level - 1) * 0.5f;  // 2, 2.5, 3 seconds
            break;
    }
    
    return stats;
}

// ============================================================================
// ENEMY STATS IMPLEMENTATION
// ============================================================================

EnemyStats GetEnemyStats(EnemyType type) {
    EnemyStats stats = {};
    
    switch (type) {
        case EnemyType::WEAK:
            stats.maxHealth = 20;
            stats.speed = 1.5f;  // tiles per second
            stats.moneyReward = 5;
            stats.baseDamage = 1;
            break;
            
        case EnemyType::NORMAL:
            stats.maxHealth = 50;
            stats.speed = 1.0f;
            stats.moneyReward = 10;
            stats.baseDamage = 2;
            break;
            
        case EnemyType::FAST:
            stats.maxHealth = 30;
            stats.speed = 2.5f;
            stats.moneyReward = 15;
            stats.baseDamage = 1;
            break;
            
        case EnemyType::TANK:
            stats.maxHealth = 200;
            stats.speed = 0.7f;
            stats.moneyReward = 25;
            stats.baseDamage = 5;
            break;
            
        case EnemyType::BOSS:
            stats.maxHealth = 500;
            stats.speed = 0.5f;
            stats.moneyReward = 100;
            stats.baseDamage = 10;
            break;
    }
    
    return stats;
}

// ============================================================================
// WAVE GENERATION IMPLEMENTATION
// ============================================================================

WaveComposition GenerateWaveComposition(int waveNumber) {
    WaveComposition comp = {};
    
    // Difficulty scaling factor
    float difficultyScale = 1.0f + (waveNumber / 5) * 0.2f;  // +20% every 5 waves
    
    // Boss waves (every 5th wave)
    if (waveNumber % 5 == 0) {
        comp.bossCount = 1 + (waveNumber / 10);  // More bosses on later waves
        comp.tankCount = static_cast<int>(2 * difficultyScale);
        comp.normalCount = static_cast<int>(5 * difficultyScale);
        comp.fastCount = static_cast<int>(3 * difficultyScale);
        comp.weakCount = 0;
    }
    // Early waves (1-5)
    else if (waveNumber <= 5) {
        comp.bossCount = 0;
        comp.tankCount = 0;
        comp.fastCount = waveNumber;
        comp.normalCount = waveNumber * 2;
        comp.weakCount = waveNumber * 3;
    }
    // Mid waves (6-15)
    else if (waveNumber <= 15) {
        comp.bossCount = 0;
        comp.tankCount = static_cast<int>(1 * difficultyScale);
        comp.fastCount = static_cast<int>(3 * difficultyScale);
        comp.normalCount = static_cast<int>(8 * difficultyScale);
        comp.weakCount = static_cast<int>(5 * difficultyScale);
    }
    // Late waves (16+)
    else {
        comp.bossCount = 0;
        comp.tankCount = static_cast<int>(3 * difficultyScale);
        comp.fastCount = static_cast<int>(5 * difficultyScale);
        comp.normalCount = static_cast<int>(10 * difficultyScale);
        comp.weakCount = static_cast<int>(8 * difficultyScale);
    }
    
    return comp;
}

// ============================================================================
// TOWER COST CALCULATIONS
// ============================================================================

int GetTowerUpgradeCost(TowerType type, int currentLevel) {
    if (currentLevel >= 3) return 0;  // Max level reached
    
    TowerStats stats = GetTowerStats(type, currentLevel + 1);
    return stats.upgradeCost;
}

int GetTowerSellValue(TowerType type, int currentLevel) {
    // Calculate total investment
    int totalCost = 0;
    
    TowerStats baseStats = GetTowerStats(type, 1);
    totalCost += baseStats.cost;
    
    // Add upgrade costs
    for (int level = 1; level < currentLevel; level++) {
        TowerStats stats = GetTowerStats(type, level);
        totalCost += stats.upgradeCost;
    }
    
    // Return 50% of total investment
    return totalCost / 2;
}

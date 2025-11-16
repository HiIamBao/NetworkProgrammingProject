# Dead Player Fix - Horde Defense Mode

## Date
November 8, 2025

## Issues Fixed

### 1. Dead Players Can Still Shoot and Damage Monsters
**Problem:** Dead players (HP = 0) could continue shooting bullets and damaging enemies in Horde Defense mode.

**Root Cause:** 
- Client-side shooting logic didn't check if player was alive before firing
- Server-side bullet processing didn't validate player health status
- Bullet-enemy collision handling didn't check if the shooter was alive

**Solution:**
- Added `player.life > 0` check in client shooting logic (both normal and battery shooting)
- Added validation in server's `headerSendBullet` handler to reject bullets from dead players
- Added validation in server's `headerHordeBulletHitEnemy` handler to reject damage from dead players

**Files Modified:**
- `src/gameLayer/client.cpp` - Lines 807-863 (shooting logic)
- `src/gameLayer/server.cpp` - Lines 305-323 (bullet broadcasting)
- `src/gameLayer/server.cpp` - Lines 478-495 (bullet-enemy collision)

### 2. No Death Screen for Dead Players
**Problem:** Dead players had no visual feedback that they were dead, just lost control of their character.

**Solution:** 
Added a death screen overlay for Horde Defense mode that displays:
- Red semi-transparent overlay to indicate death state
- "YOU DIED" message in red text
- Status message explaining respawn conditions
- Current wave information during wave phase
- Instructions to leave match (ESC key)

**Files Modified:**
- `src/gameLayer/client.cpp` - Lines 1566-1595 (death screen UI)

### 3. Dead Players Not Respawning After Wave Complete
**Problem:** When a wave completed, dead players were marked for respawn but immediately marked as dead again, preventing respawn. Additionally, respawned players only got base HP (5) instead of their upgraded max HP.

**Root Causes:** 
1. **Race condition in server update loop:**
   - Wave completes → `respawnAllDeadPlayers()` marks players as alive
   - Death check loop sees `life <= 0` AND `isPlayerAlive() == true`
   - Immediately calls `markPlayerDead()`, undoing the respawn flag
   - Respawn loop never triggers because player is no longer marked alive

2. **Incorrect HP restoration:**
   - `respawnPlayer()` was using `player.maxLife` (static constant = 5)
   - Should use `getEffectiveMaxHealth(player)` to account for health upgrades
   - This caused respawned players to have only 5 HP even with health upgrades

**Solutions:**
1. Added `!needsRespawn()` check to death detection logic to prevent marking players as dead if they're waiting for respawn
2. Changed `respawnPlayer()` to use `getEffectiveMaxHealth(player)` instead of `player.maxLife`

This ensures:
- During wave phase: Players who die are marked as dead normally
- During wave complete: Players waiting for respawn are not re-marked as dead
- Respawn loop can properly execute and restore player HP to their full upgraded maximum
- Players with health upgrades respawn with the correct amount of HP

**Files Modified:**
- `src/gameLayer/server.cpp` - Lines 738-752 (death detection logic)
- `src/gameLayer/HordeDefenseManager.cpp` - Line 398 (respawn HP calculation)

## Implementation Details

### Client-Side Shooting Prevention
```cpp
// Only allow shooting if player is alive
if ((platform::isLMouseHeld() || platform::getControllerButtons().LT > CONTROLLER_MARGIN)
    && culldown <= 0.f
    && player.life > 0)  // NEW: Check if player is alive
{
    // ... shooting logic
}
```

### Server-Side Bullet Validation
```cpp
else if (p.header == headerSendBullet)
{
    // Check if the player is alive before processing bullet
    auto playerIt = instance->connections.find(p.cid);
    if (playerIt != instance->connections.end())
    {
        if (playerIt->second.entityData.life <= 0)
        {
            std::cout << "Rejected bullet from dead player CID " << p.cid << std::endl;
            return;
        }
    }
    // ... broadcast bullet
}
```

### Server-Side Damage Validation
```cpp
else if (p.header == headerHordeBulletHitEnemy)
{
    // ... existing checks
    
    // Check if player is alive - dead players can't damage enemies
    if (playerIt->second.entityData.life <= 0)
    {
        std::cout << "Rejected bullet damage from dead player CID " << p.cid << std::endl;
        return;
    }
    
    // ... calculate and apply damage
}
```

### Death Screen UI
```cpp
// Display death screen (Horde Defense only)
if (player.life <= 0 && currentGameMode == GameMode::HORDE_DEFENSE)
{
    // Red overlay
    renderer.renderRectangle(overlayPos, {0.3f, 0.0f, 0.0f, 0.6f});
    
    // "YOU DIED" message
    renderer.renderText(deathPos, "YOU DIED", textures.font, 
        glm::vec4(1.0f, 0.2f, 0.2f, 1.0f), 1.2f);
    
    // Status and wave info
    // ... 
}
```

### Respawn Race Condition Fix
```cpp
// Check for NEW player deaths (alive flag is true AND HP just dropped to 0)
// BUT ONLY during wave phases (not during buying phase when players respawn)
for (auto& conn : instance->connections)
{
    if (conn.second.entityData.life <= 0 && 
        instance->hordeDefenseManager->isPlayerAlive(conn.first) &&
        !instance->hordeDefenseManager->needsRespawn(conn.first))  // NEW: Don't mark dead if waiting for respawn
    {
        // Mark player as dead
        instance->hordeDefenseManager->markPlayerDead(conn.first);
    }
}
```

### Respawn HP Calculation Fix
```cpp
void HordeDefenseManager::respawnPlayer(int32_t cid, phisics::Entity& player) {
    playerAlive[cid] = true;
    playerRespawned[cid] = true;
    player.life = getEffectiveMaxHealth(player);  // NEW: Use effective max HP with upgrades
    // ... reset buffs and position
}

int HordeDefenseManager::getEffectiveMaxHealth(const phisics::Entity& player) const {
    UpgradeInfo info = UpgradeInfo::getInfo(UpgradeType::HEALTH);
    int maxHP = player.maxLife + (int)(player.healthUpgradeLevel * info.effectPerLevel);
    maxHP += player.tempMaxHealthBoost;
    return maxHP;  // Base 5 + upgrades + temp boosts
}
```

## Testing

### Test Scenarios
1. **Dead Player Shooting Prevention**
   - Start Horde Defense mode
   - Let an enemy kill you (HP = 0)
   - Try to shoot - no bullets should be fired
   - Verify death screen appears

2. **Death Screen Display**
   - Die during a wave
   - Verify death screen shows:
     - Red overlay
     - "YOU DIED" message
     - "Spectating..." message
     - Current wave info
     - ESC instruction

3. **Wave Complete Respawn**
   - Die during a wave
   - Wait for wave to complete (all enemies dead)
   - Verify you respawn with full HP at the start of buying phase
   - Verify you can shoot and move normally after respawn

4. **Server-Side Validation**
   - Monitor server console output
   - Dead player shooting attempts should show "Rejected bullet from dead player CID X"
   - Dead player damage attempts should show "Rejected bullet damage from dead player CID X"

## Related Files
- `src/gameLayer/client.cpp` - Client shooting logic and death screen UI
- `src/gameLayer/server.cpp` - Server validation and respawn logic
- `src/gameLayer/HordeDefenseManager.cpp` - Respawn state management
- `include/gameLayer/HordeDefenseManager.h` - Respawn method declarations

## Future Enhancements
- Add death animation/effect
- Add spectator camera that follows living players
- Add death statistics (time survived, enemies killed, etc.)
- Add resurrection items or abilities
- Add penalty for death (lose money, etc.)

## Build Status
✅ All changes compiled successfully
✅ No new warnings introduced
✅ Ready for testing

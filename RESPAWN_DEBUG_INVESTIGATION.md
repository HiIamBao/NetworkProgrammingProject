# Respawn Debug Investigation

## Date
November 8, 2025 (Update 3)

## Issue
Dead players are not respawning after wave completion. The death screen persists and players remain at 0 HP.

## Debug Logging Added

### Server-side (server.cpp lines 723-739)
Added detailed logging in the respawn loop:
```cpp
// Before respawn check
std::cout << "[HordeDefense] Player " << conn.first << " needs respawn! Current HP: " << conn.second.entityData.life << std::endl;

// After respawn
std::cout << "[HordeDefense] Player " << conn.first << " HP after respawn: " << conn.second.entityData.life << std::endl;
std::cout << "[HordeDefense] Player " << conn.first << " respawned for new wave (HP: " << conn.second.entityData.life << ")" << std::endl;
```

### Manager-side (HordeDefenseManager.cpp)

**respawnAllDeadPlayers()** (lines 456-467):
```cpp
std::cout << "[HordeDefense] respawnAllDeadPlayers() called" << std::endl;
std::cout << "[HordeDefense] Player " << cid << " alive status: " << alive << std::endl;
std::cout << "[HordeDefense] Player " << cid << " marked for respawn (alive=true, respawned=false)" << std::endl;
```

**respawnPlayer()** (lines 395-427):
```cpp
std::cout << "[HordeDefense] respawnPlayer() called for CID " << cid << ", current HP: " << player.life << std::endl;
std::cout << "[HordeDefense] Calculated max HP for player " << cid << ": " << newHP << std::endl;
std::cout << "[HordeDefense] Player " << cid << " respawned with HP: " << player.life << " at position (...)" << std::endl;
```

## Expected Log Flow (When Working Correctly)

### 1. Player Dies During Wave
```
[HordeDefense] Player 1 died!
```

### 2. Wave Completes
```
[HordeDefense] Wave X complete! Bonus: $XXX
[HordeDefense] respawnAllDeadPlayers() called
[HordeDefense] Player 1 alive status: 0
[HordeDefense] Player 1 marked for respawn (alive=true, respawned=false)
[HordeDefense] State change: 3 -> 1  (WAVE_COMPLETE -> BUYING_PHASE)
```

### 3. Server Update Loop (Next Frame)
```
[HordeDefense] Player 1 needs respawn! Current HP: 0
[HordeDefense] respawnPlayer() called for CID 1, current HP: 0
[HordeDefense] Calculated max HP for player 1: 5 (or higher with upgrades)
[HordeDefense] Player 1 respawned with HP: 5 at position (X, Y)
[HordeDefense] Player 1 HP after respawn: 5
[HordeDefense] Player 1 respawned for new wave (HP: 5)
```

### 4. Client Receives Update
- Entity update packet with `life = 5` (or upgraded HP)
- Death screen check: `player.life <= 0` → FALSE → Death screen disappears
- Shooting check: `player.life > 0` → TRUE → Can shoot

## Diagnostic Questions

Based on the logs, we can determine:

### Q1: Is respawnAllDeadPlayers() being called?
**Look for:** `[HordeDefense] respawnAllDeadPlayers() called`
- **YES** → Continue to Q2
- **NO** → Issue: Wave completion not calling respawn function

### Q2: Is the player marked for respawn?
**Look for:** `[HordeDefense] Player X marked for respawn (alive=true, respawned=false)`
- **YES** → Continue to Q3
- **NO** → Issue: Player not in dead state when wave completes

### Q3: Is needsRespawn() returning true?
**Look for:** `[HordeDefense] Player X needs respawn! Current HP: 0`
- **YES** → Continue to Q4
- **NO** → Issue: respawn condition check failing
  - Possible causes:
    - `life > 0` (player not actually dead)
    - `needsRespawn()` returning false (logic error)

### Q4: Is respawnPlayer() being called?
**Look for:** `[HordeDefense] respawnPlayer() called for CID X, current HP: 0`
- **YES** → Continue to Q5
- **NO** → Issue: Respawn loop not executing

### Q5: Is HP being set correctly?
**Look for:** `[HordeDefense] Calculated max HP for player X: Y`
- **If Y = 0** → Issue: getEffectiveMaxHealth() returning 0
- **If Y > 0** → Continue to Q6

### Q6: Is HP persisting after assignment?
**Look for:** 
- `[HordeDefense] Player X HP after respawn: Y`
- **If Y = 0** → Issue: HP assignment not working (reference problem?)
- **If Y > 0** → Continue to Q7

### Q7: Is the entity update being broadcast?
**Server should broadcast** the updated entity to all clients
- Check if `conn.second.changed = true` is set
- Check if clients receive `headerUpdateConnection` packet

## Possible Root Causes

### 1. Respawn Not Triggered
- Wave completion not calling `respawnAllDeadPlayers()`
- State transition bypassing respawn logic

### 2. Condition Check Failing
- `needsRespawn()` logic incorrect
- Player not actually marked as dead
- Race condition with death check

### 3. HP Not Set Correctly
- `getEffectiveMaxHealth()` returning 0
- Assignment to wrong entity reference
- Entity not being copied/updated correctly

### 4. Update Not Broadcast
- `conn.second.changed` not set
- Broadcast not triggered
- Client not receiving update

### 5. Client Not Processing Update
- Client ignores update for some reason
- Update arrives but player reference not updated
- Death screen check runs before update processed

## Testing Instructions

1. **Start Horde Defense mode** (single player is fine)
2. **Let an enemy kill you** (HP = 0, death screen appears)
3. **Wait for wave to complete** (all enemies dead)
4. **Watch console output** for the log messages above
5. **Check if:**
   - Death screen disappears
   - Can shoot and move
   - HP bar shows full HP
6. **Share the console output** from step 4

## Next Steps

Based on the log output, we will:
1. Identify which step is failing
2. Fix the specific issue
3. Remove debug logging once fixed
4. Document the root cause and solution

## Build Status
✅ Debug version compiled successfully
✅ Enhanced logging active
🔍 Ready for testing and diagnosis

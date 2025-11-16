# Death & Respawn System - Quick Reference

## How It Works

### Death Detection
```
Player HP reaches 0 → markPlayerDead() → Death screen appears
```

### Death State
- **Can't shoot:** Both client and server prevent bullet creation
- **Can't damage:** Server rejects all damage from dead players  
- **Can still move:** Player can navigate (spectate mode)
- **Visual feedback:** Red death screen overlay with status info

### Respawn Flow
```
Wave Complete → respawnAllDeadPlayers() → 
Set alive=true, respawned=false → 
Server detects needsRespawn()=true → 
Calls respawnPlayer() → 
Restore full HP (with upgrades!) → markPlayerRespawned()
```

### Key Respawn Details
- **HP Restoration:** Uses `getEffectiveMaxHealth(player)` to include health upgrades
  - Base HP: 5
  - Per health upgrade: +2 HP (or whatever effectPerLevel is)
  - Temp boosts: Added on top
  - Example: Level 3 health upgrade = 5 + (3 * 2) = 11 HP
- **Position:** Randomized spawn location
- **Buffs:** All temporary buffs are cleared on respawn

### Key Checks

#### Client Shooting Check
```cpp
if (player.life > 0) {
    // Allow shooting
}
```

#### Server Bullet Validation
```cpp
if (playerIt->second.entityData.life <= 0) {
    return;  // Reject
}
```

#### Death Detection (Server)
```cpp
if (life <= 0 && isPlayerAlive() && !needsRespawn()) {
    markPlayerDead();
}
```

#### Respawn Check (Server)
```cpp
if (life <= 0 && needsRespawn()) {
    respawnPlayer();
    markPlayerRespawned();
}
```

## State Flags

### `playerAlive` Map
- `true` = Player is alive (or waiting for respawn)
- `false` = Player is dead (will respawn next wave)

### `playerRespawned` Map
- `true` = Player already respawned this wave
- `false` = Player needs respawn (if alive flag is set)

## Death Screen Messages

### During Wave
```
YOU DIED
Spectating... You will respawn next wave
Wave X/20 in progress
Press ESC to leave match
```

### During Buying Phase (After Death)
```
YOU DIED
You will respawn at the start of the next wave
Press ESC to leave match
```

## Common Issues & Solutions

### Issue: Player respawns but can't shoot
**Cause:** Client-side life not updated
**Solution:** Ensure `conn.second.changed = true` after respawn

### Issue: Player doesn't respawn after wave
**Cause:** Death check marking player dead during respawn window
**Solution:** Added `!needsRespawn()` check to prevent re-marking

### Issue: Player respawns with only 5 HP despite health upgrades
**Cause:** Using `player.maxLife` (static constant) instead of effective max HP
**Solution:** Changed to `getEffectiveMaxHealth(player)` in respawn function

### Issue: Dead player bullets still damage enemies
**Cause:** Server not validating shooter health
**Solution:** Added health check in `headerHordeBulletHitEnemy`

## Console Output

### Death
```
[HordeDefense] Player 1 died!
```

### Respawn
```
[HordeDefense] Player 1 marked for respawn
[HordeDefense] Player 1 respawned for new wave
```

### Rejected Actions
```
Rejected bullet from dead player CID 1
Rejected bullet damage from dead player CID 1
```

## Files Reference
- **Client Shooting:** `client.cpp:807-920`
- **Death Screen:** `client.cpp:1566-1595`
- **Server Validation:** `server.cpp:305-323, 478-495`
- **Respawn Logic:** `server.cpp:723-737`
- **Death Detection:** `server.cpp:738-752`
- **Manager Methods:** `HordeDefenseManager.cpp:395-425, 440-480`
- **Effective Max HP:** `HordeDefenseManager.cpp:912-918`

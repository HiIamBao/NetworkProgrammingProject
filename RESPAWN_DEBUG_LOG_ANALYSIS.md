# Respawn Debug Log Analysis

## Issue
After wave completion, dead players are respawned but immediately marked as dead again, causing:
- Death screen remains visible
- Players cannot shoot
- HP shows 0 on client

## Previous Logs Showed
```
[HordeDefense] Player 2 respawned with HP: 5
[HordeDefense] Player 2 HP after respawn: 5
[HordeDefense] Player 2 marked as dead  <-- PROBLEM!
```

## New Debug Logging Added

### 1. headerUpdateConnection logging (line ~300)
Shows when client sends position updates and whether server HP is preserved:
```
[UpdateConn] CID X - Client HP: Y, Server HP (before): Z
[UpdateConn] CID X - Server HP (after): Z (unchanged)
```

### 2. Death check logging (line ~765)
Shows all death check conditions for every player:
```
[DeathCheck] CID X - HP: Y, isAlive: true/false, needsRespawn: true/false
```

## Hypothesis
The death check is triggering because:
1. Player respawns with HP=5 ✓
2. Client sends headerUpdateConnection with old state (HP=0)
3. **Even though we don't copy the life field**, something else is setting it back to 0

## What to Look For in New Logs

### Expected Sequence (CORRECT)
```
[HordeDefense] Player X respawned with HP: 5
[UpdateConn] CID X - Client HP: 0, Server HP (before): 5
[UpdateConn] CID X - Server HP (after): 5 (unchanged)
[DeathCheck] CID X - HP: 5, isAlive: true, needsRespawn: false
```

### Bug Sequence (INCORRECT - if this happens)
```
[HordeDefense] Player X respawned with HP: 5
[UpdateConn] CID X - Client HP: 0, Server HP (before): 5
[UpdateConn] CID X - Server HP (after): 0  <-- BUG: HP changed!
[DeathCheck] CID X - HP: 0, isAlive: true, needsRespawn: false
[HordeDefense] Player X marked as dead
```

## Test Steps
1. Start server in Horde Defense mode
2. Join with 1-2 players
3. Let wave 1 start
4. Let a player die (HP reaches 0)
5. Complete wave 1 (kill all enemies)
6. **Watch the logs carefully** during buying phase
7. When wave 2 starts, check if dead players respawn correctly

## Next Steps Based on Results

### If Server HP stays at 5:
- The headerUpdateConnection protection is working
- Bug is elsewhere - check death check logic or HordeDefenseManager state

### If Server HP changes to 0:
- Something else is overwriting the HP field
- Check for other packet handlers or entity updates
- Check if broadcast/receive creates a feedback loop

### If needsRespawn shows true when it should be false:
- The respawn flag isn't being set correctly
- Check markPlayerRespawned() calls

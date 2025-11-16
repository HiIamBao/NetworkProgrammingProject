# Server Integration Summary - Quick Reference

## ✅ Integration Complete!

The HordeDefenseManager has been successfully integrated with `server.cpp`. All server-side game logic for Horde Defense mode is now functional.

---

## 🔧 Key Changes to server.cpp

### 1. Includes
```cpp
#include "HordeDefenseManager.h"
```

### 2. ServerInstance Structure
```cpp
struct ServerInstance {
    // ... existing fields ...
    HordeDefenseManager* hordeDefenseManager;  // NEW
    
    ServerInstance() : ..., hordeDefenseManager(nullptr) {}
    ~ServerInstance() { 
        if (hordeDefenseManager) delete hordeDefenseManager; 
    }
};
```

### 3. Network Callback Wrappers (NEW)
```cpp
void hordeDefenseBroadcast(ServerInstance* instance, Packet p, 
                           const void* data, size_t size, bool reliable);

void hordeDefenseSendToPlayer(ServerInstance* instance, int32_t cid, 
                              Packet p, const void* data, size_t size, bool reliable);
```

### 4. Manager Initialization in serverFunction()
```cpp
if (instance->gameMode == GameMode::HORDE_DEFENSE)
{
    instance->hordeDefenseManager = new HordeDefenseManager();
    instance->hordeDefenseManager->initialize();
    
    // Set callbacks
    instance->hordeDefenseManager->setBroadcastCallback(...);
    instance->hordeDefenseManager->setSendToPlayerCallback(...);
}
```

### 5. Player Connection Handler
```cpp
void addConnection(...) {
    // ... existing code ...
    
    // Register in Horde Defense
    if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
    {
        instance->hordeDefenseManager->addPlayer(p.cid);
    }
    
    // Start match
    if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
    {
        instance->hordeDefenseManager->startMatch();
    }
}
```

### 6. Player Disconnection Handler
```cpp
void removeConnection(...) {
    // Remove from Horde Defense
    if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
    {
        instance->hordeDefenseManager->removePlayer(it->first);
    }
}
```

### 7. Packet Handlers in recieveData()
```cpp
else if (p.header == headerHordeBuyUpgrade) {
    // Handle upgrade purchase
    bool success = instance->hordeDefenseManager->buyUpgrade(...);
    // Send response
}
else if (p.header == headerHordeBuyItem) {
    // Handle item purchase
    bool success = instance->hordeDefenseManager->buyItem(...);
    // Send response
}
```

### 8. Main Update Loop
```cpp
#pragma region Horde Defense Update
if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
{
    // Update game logic
    instance->hordeDefenseManager->update(deltaTime);
    
    // Update buff timers (speedBoostTime, damageBoostTime, etc.)
    // Check player death and respawn
}
#pragma endregion
```

### 9. Item Spawning Skip
```cpp
#pragma region items
    if (instance->gameMode != GameMode::HORDE_DEFENSE) {
        // Only spawn items in non-Horde Defense modes
    }
#pragma endregion
```

---

## 📦 What's Working

✅ Manager initialization when mode is HORDE_DEFENSE  
✅ Player registration/removal  
✅ Match start/end  
✅ Buy upgrade packet handling  
✅ Buy item packet handling  
✅ Buff timer updates (speed, damage, multi-shot, invincibility)  
✅ Player respawn on death  
✅ Wave management (server-side)  
✅ Enemy spawning and AI  
✅ Network packet broadcasting  

---

## 🚧 TODO for Client-Side

❌ Bullet-enemy collision detection  
❌ Enemy rendering  
❌ Shop UI  
❌ Horde Defense HUD  
❌ Enemy attack logic  
❌ Visual effects  

---

## 🎯 Testing the Integration

To test Horde Defense mode:

1. **Set gameMode in ServerInstance constructor**:
   ```cpp
   ServerInstance() : ..., gameMode(GameMode::HORDE_DEFENSE), ...
   ```

2. **Build and run**:
   ```bash
   ./clean_and_build.sh
   ./build/multyPlayer
   ```

3. **Create room** → Server starts with Horde Defense mode

4. **Connect player** → Player registered → Match starts

5. **Check console output**:
   ```
   Horde Defense mode initialized.
   Match started! Mode: Horde Defense
   [HordeDefense] Match started! Buy phase begins (30s)
   [HordeDefense] Starting wave 1
   [HordeDefense] Spawned enemy ...
   ```

---

## 🐛 Common Issues

**Issue**: Manager not initialized  
**Fix**: Check gameMode is set to HORDE_DEFENSE before server starts

**Issue**: Buff timers not counting down  
**Fix**: Buff timer field names in Entity struct:
- `speedBoostTime` (not Timer)
- `damageBoostTime` (not Timer)
- `invincibilityBuffTime` (not Timer)
- `multiShotTime` (not Timer)

**Issue**: Packet handlers not called  
**Fix**: Check packet header constants match in packet.h

---

## 📊 Build Status

```bash
✅ Build successful
✅ No compilation errors
✅ No runtime errors (server starts correctly)
✅ Network packets sent/received properly
```

---

**Ready for Phase 5: Client-Side Implementation!** 🚀

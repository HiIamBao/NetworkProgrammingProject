# Phase 5: Client-Side Implementation Plan

## 🎯 Objective
Implement client-side rendering and logic for Horde Defense mode so players can see and interact with the game.

---

## 📋 Implementation Tasks

### 1. Client-Side Data Structures ⏳
**File**: `client.cpp`
**Tasks**:
- Add enemy list storage (`std::vector<HordeDefense::Enemy>`)
- Add Horde Defense state variables
- Add player money tracking
- Add active buff tracking
- Add wave/timer display data

### 2. Packet Receivers ⏳
**File**: `client.cpp` (in `msgLoop()`)
**Add handlers for**:
- `headerHordeStateUpdate` → Update wave, timer, state
- `headerHordeSpawnEnemy` → Add enemy to local list
- `headerHordeEnemyUpdate` → Update enemy positions/health
- `headerHordeEnemyDeath` → Remove enemy, show effect
- `headerHordeWaveStart` → Show wave start notification
- `headerHordeWaveComplete` → Show wave complete message
- `headerHordePlayerMoneyUpdate` → Update player money
- `headerHordePlayerStatsUpdate` → Update player upgrades/buffs
- `headerHordePlayerRespawn` → Respawn player
- `headerHordeMatchEnd` → Show victory/defeat screen

### 3. Enemy Rendering ⏳
**File**: `client.cpp` (in `clientFunction()`)
**Tasks**:
- Draw enemies at their positions
- Show health bars above enemies
- Color code by enemy type
- Handle death animations

### 4. Horde Defense HUD ⏳
**File**: `client.cpp` (in `clientFunction()`)
**Tasks**:
- Wave number display (top center)
- Buy phase timer countdown
- Player money display (top right)
- Active buffs indicator with timers
- Enemy count remaining

### 5. Shop UI ⏳
**File**: `client.cpp` (in `clientFunction()`)
**Tasks**:
- Create shop menu (press 'B' during buy phase)
- Show upgrades with levels and costs
- Show items with prices and descriptions
- Send buy requests to server
- Handle buy responses (success/failure messages)
- Show current money

### 6. Bullet-Enemy Collision (CRITICAL) ⏳
**Option A: Client-Side with Validation**
- Detect collision client-side
- Send packet to server for validation
- Server processes damage and broadcasts

**Option B: Server-Side Only**
- Server intercepts all bullets
- Server checks collision with enemies
- Already have `damageEnemy()` function

**Recommendation**: Option A for better responsiveness

### 7. Wave Notifications ⏳
**File**: `client.cpp`
**Tasks**:
- "Wave X Starting!" overlay (3s)
- "Wave Complete! Bonus: $XXX" overlay (3s)
- "Buy Phase - 30s remaining" display
- Victory/Defeat screens

---

## 📁 Files to Modify

1. **`/src/gameLayer/client.cpp`** (main file)
   - Add Horde Defense state variables
   - Add packet handlers
   - Add enemy rendering
   - Add HUD rendering
   - Add shop UI

2. **`/include/gameLayer/serverClient.h`** (if needed)
   - Add any necessary client-side declarations

---

## 🎨 UI Layout Plan

### In-Game HUD (Horde Defense):
```
┌─────────────────────────────────────────────────┐
│  Wave: 5/20    Buy Phase: 27s    Money: $1,250  │  <- Top bar
│                                                  │
│  [Buffs: ⚡15s 🛡25s]                           │  <- Buff icons
│                                                  │
│                                                  │
│         [Game viewport - enemies/players]        │
│                                                  │
│                                                  │
│  Enemies: 8                                      │  <- Bottom left
└─────────────────────────────────────────────────┘
```

### Shop Menu (Press 'B'):
```
┌──────────── SHOP ────────────┐
│                              │
│ === UPGRADES ===             │
│ [Damage]      Lv 2  $150    │
│ [Fire Rate]   Lv 1  $100    │
│ [Health]      Lv 0  $100    │
│ [Speed]       Lv 1  $100    │
│ [Bullet Speed] Lv 0  $100   │
│                              │
│ === ITEMS ===                │
│ [Health Pack]      $50      │
│ [Shield]           $150     │
│ [Speed Boost 30s]  $100     │
│ [Damage Amp 20s]   $200     │
│ [Invincibility 5s] $500     │
│                              │
│ Money: $750                  │
│ [Close - Press B]            │
└──────────────────────────────┘
```

---

## 🎯 Implementation Order (Priority)

### Phase 5A: Basic Packet Handling (HIGH PRIORITY)
1. Add client-side data structures
2. Implement all packet receivers
3. Update client state based on packets
**Estimated Time**: 1 hour

### Phase 5B: Enemy Rendering (HIGH PRIORITY)
1. Draw enemies on screen
2. Show health bars
3. Handle death animations
**Estimated Time**: 45 minutes

### Phase 5C: Horde Defense HUD (MEDIUM PRIORITY)
1. Wave/timer display
2. Money display
3. Buff indicators
4. Enemy count
**Estimated Time**: 45 minutes

### Phase 5D: Shop UI (MEDIUM PRIORITY)
1. Create shop menu
2. Send buy requests
3. Handle responses
**Estimated Time**: 1 hour

### Phase 5E: Bullet-Enemy Collision (CRITICAL)
1. Detect collisions
2. Send damage packets
3. Handle validation
**Estimated Time**: 30 minutes

### Phase 5F: Visual Polish (LOW PRIORITY)
1. Wave notifications
2. Match end screens
3. Visual effects
**Estimated Time**: 1 hour

---

## 🔧 Technical Details

### Enemy Storage:
```cpp
// In client.cpp (static variables)
static std::map<int32_t, HordeDefense::Enemy> hordeEnemies;
static HordeDefense::HordeDefenseState hordeState = HordeDefense::HordeDefenseState::WAITING;
static int currentWave = 0;
static float phaseTimer = 0.0f;
static int playerMoney = 0;
```

### Packet Handler Example:
```cpp
else if (p.header == headerHordeSpawnEnemy)
{
    auto spawnData = *(HordeSpawnEnemyData*)data;
    HordeDefense::Enemy enemy;
    enemy.id = spawnData.enemyId;
    enemy.type = spawnData.enemyType;
    enemy.position = spawnData.position;
    enemy.health = spawnData.health;
    enemy.maxHealth = spawnData.maxHealth;
    hordeEnemies[enemy.id] = enemy;
}
```

### Enemy Rendering Example:
```cpp
// Draw enemies
for (const auto& [id, enemy] : hordeEnemies)
{
    glm::vec4 enemyRect = {
        enemy.position.x, 
        enemy.position.y, 
        1.0f, 1.0f
    };
    
    // Color based on type
    glm::vec4 color = getEnemyColor(enemy.type);
    renderer.renderRectangle(enemyRect, color);
    
    // Health bar
    float healthPercent = enemy.health / enemy.maxHealth;
    // ... draw health bar ...
}
```

---

## ⚠️ Important Notes

1. **Game Mode Check**: Always check if `currentGameMode == GameMode::HORDE_DEFENSE` before rendering Horde Defense UI
2. **Null Checks**: Verify data exists before accessing
3. **Network Sync**: Client is display-only, server is authoritative
4. **Performance**: Enemy updates at 10Hz (unreliable), state updates on change (reliable)

---

## 🧪 Testing Plan

1. **Test packet reception**: Add console logs for each packet type
2. **Test enemy rendering**: Verify enemies appear at correct positions
3. **Test HUD updates**: Check wave/timer/money display
4. **Test shop**: Verify purchases work and money updates
5. **Test collision**: Shoot enemies, verify health decreases
6. **Test full match**: Play through multiple waves

---

## 📊 Success Criteria

- ✅ All 10 packet types handled correctly
- ✅ Enemies render at correct positions
- ✅ HUD displays current game state
- ✅ Shop UI functional and sends requests
- ✅ Bullets hit enemies
- ✅ Wave progression visible
- ✅ Victory/defeat screens work

---

**Let's begin with Phase 5A: Basic Packet Handling!**

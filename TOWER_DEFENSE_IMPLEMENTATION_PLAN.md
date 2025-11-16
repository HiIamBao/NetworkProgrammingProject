# Horde Defense Game Mode - Implementation Plan

## 📋 Overview
Implement a cooperative Horde Defense mode where players work together to survive waves of AI enemies. Players must defend themselves directly using weapons, abilities, and tactical positioning. Earn money from kills to buy better weapons and upgrades.

---

## 🎯 Game Concept

### Core Mechanics
- **Cooperative Survival**: All players work together against waves of AI enemies
- **Wave-Based**: Enemies spawn in waves with increasing difficulty
- **Direct Combat**: Players shoot and fight enemies themselves (no towers)
- **Weapon/Upgrade System**: Earn money from kills to buy better weapons, health, ammo, and perks
- **Safe Zones**: Designated areas where players can buy items between waves
- **Shared Resources**: Team money pool or individual money (configurable)

### Victory Conditions
- ✅ **Win**: Survive all waves (e.g., 20 waves)
- ❌ **Lose**: All players die

### Key Differences from Deathmatch
- **PvE (Player vs Environment)**: Fight AI enemies, not each other
- **Respawn System**: Dead players respawn at the start of next wave
- **Shop System**: Buy weapons, ammo, health between waves
- **Progressive Difficulty**: Enemies get stronger each wave

---

## 📊 Implementation Checklist

### Phase 1: Data Structures & Enums ✓
- [ ] 1.1 Add `HORDE_DEFENSE` to GameMode enum
- [ ] 1.2 Create Enemy AI types enum (ZOMBIE, RUNNER, TANK, EXPLODER, BOSS)
- [ ] 1.3 Create HordeDefenseState enum (WAITING, BUYING_PHASE, WAVE_ACTIVE, WAVE_COMPLETE, VICTORY, DEFEAT)
- [ ] 1.4 Create UpgradeType enum (DAMAGE, FIRE_RATE, HEALTH, SPEED, BULLET_SPEED)
- [ ] 1.5 Create ShopItemType enum (HEALTH_PACK, MAX_HEALTH_BOOST, SHIELD, SPEED_BOOST, DAMAGE_AMPLIFIER, INVINCIBILITY, MULTI_SHOT)
- [ ] 1.6 Define Enemy AI data structure (health, speed, damage, AI behavior)
- [ ] 1.7 Define Wave configuration structure (enemy counts, spawn rate)
- [ ] 1.8 Define Upgrade stats structure (base cost, effect multiplier, max level)
- [ ] 1.9 Define Shop item structure (cost, effect, duration)
- [ ] 1.10 Add player upgrade levels and active buffs to Entity

### Phase 2: Network Packets ✓
- [ ] 2.1 Add packet headers for horde defense
  - `headerHordeStateUpdate` (wave number, state, time remaining)
  - `headerSpawnEnemy` (enemy type, position, ID)
  - `headerEnemyUpdate` (position, health, state)
  - `headerEnemyDeath` (killer ID, money reward)
  - `headerWaveStart` (wave number, enemy counts)
  - `headerWaveComplete` (performance stats, bonus money)
  - `headerPlayerMoneyUpdate` (player money)
  - `headerBuyUpgradeRequest` (upgrade type, level)
  - `headerBuyUpgradeResponse` (success/failure, new stats)
  - `headerBuyItemRequest` (item type)
  - `headerBuyItemResponse` (success/failure, buff applied)
  - `headerPlayerRespawn` (player ID, respawn position)
  - `headerShopOpen/Close`
- [ ] 2.2 Create packet data structures
  - `EnemySpawnData`: type, position, health, speed, id
  - `EnemyUpdateData`: id, position, health, target
  - `WaveStartData`: waveNumber, enemyCounts[], spawnRate
  - `BuyItemData`: itemType, itemID, cost
  - `MoneyUpdateData`: playerCid, newMoney, reason
- [ ] 2.3 Implement packet serialization

### Phase 3: Server-Side Logic ✓
- [ ] 3.1 Create HordeDefenseManager class
  - Wave management
  - Enemy spawning logic
  - Money/economy system
  - Shop validation
- [ ] 3.2 Implement AI enemy spawning system
  - Spawn points around the map
  - Spawn rate based on wave difficulty
  - Enemy type distribution per wave
- [ ] 3.3 Implement Enemy AI behavior
  - Pathfinding to nearest player
  - Attack logic when in range
  - Different behaviors per enemy type
- [ ] 3.4 Implement money/reward system
  - Award money on enemy kill
  - Bonus money for wave completion
  - Shared or individual money tracking
- [ ] 3.5 Implement shop system
  - Validate purchases (money check)
  - Apply weapon/item effects
  - Handle shop opening during buy phase
- [ ] 3.6 Implement wave progression logic
  - Check all enemies defeated
  - Start buy phase (30 seconds)
  - Auto-start next wave after timer
- [ ] 3.7 Implement player death/respawn
  - Mark player as dead
  - Respawn at start of next wave
  - Check if all players dead (game over)
- [ ] 3.8 Implement match end conditions
  - Victory: Survived all waves
  - Defeat: All players dead
- [ ] 3.9 Add horde defense state to server.cpp
  - Mode selection in room creation
  - State machine for wave/buy phases

### Phase 4: Client-Side Game Logic ✓
- [ ] 4.1 Create client-side enemy rendering
  - Enemy sprites/animations
  - Health bars above enemies
  - Death animations
- [ ] 4.2 Implement enemy movement interpolation
  - Smooth movement between network updates
  - Prediction for low latency
- [ ] 4.3 Handle player shooting AI enemies
  - Hit detection (already works from deathmatch)
  - Show damage numbers
  - Visual feedback on hit
- [ ] 4.4 Implement shop UI
  - Weapon selection grid
  - Item purchase buttons
  - Preview weapon stats
- [ ] 4.5 Add visual effects
  - Muzzle flashes
  - Explosion effects
  - Blood/hit effects
- [ ] 4.6 Handle network updates
  - Enemy spawn/death
  - Money updates
  - Wave state changes
- [ ] 4.7 Implement respawn screen
  - "You died" overlay
  - Wait for next wave
  - Spectate mode (optional)

### Phase 5: UI Elements ✓
- [ ] 5.1 Create Horde Defense HUD
  - Wave counter (Wave X/20)
  - Players alive counter (3/4 Alive)
  - Player money display ($450)
  - Next wave timer (30s)
  - Ammo counter (if weapon system)
- [ ] 5.2 Create Shop UI Panel
  - Weapon categories
  - Item grid with icons
  - Item name, cost, stats
  - Purchase button
  - "Not enough money" feedback
- [ ] 5.3 Create Wave Start Banner
  - "Wave X Starting!" animation
  - Enemy types preview
  - Quick countdown (3, 2, 1, GO!)
- [ ] 5.4 Create Wave Complete Banner
  - "Wave X Complete!" 
  - Performance stats (kills, accuracy)
  - Money earned
  - "Shop is open" message
- [ ] 5.5 Create End Game Screen
  - Victory: "You Survived!" with fireworks
  - Defeat: "You Died on Wave X"
  - Final stats (total kills, accuracy, damage dealt)
  - MVP player (most kills)
  - Return to lobby button
- [ ] 5.6 Create Death Screen
  - "You are dead" overlay
  - "Respawning next wave..."
  - Spectate teammates (optional)

### Phase 6: AI & Pathfinding ✓
- [ ] 6.1 Define enemy spawn points (multiple locations around map)
- [ ] 6.2 Implement basic AI pathfinding
  - Find nearest alive player
  - Move toward player
  - Avoid getting stuck on walls
- [ ] 6.3 Implement enemy attack logic
  - Melee enemies: Damage on collision
  - Ranged enemies: Shoot projectiles (optional)
- [ ] 6.4 Implement different enemy behaviors
  - **Zombie**: Slow, melee, low HP
  - **Runner**: Fast, melee, medium HP
  - **Tank**: Slow, high HP, heavy damage
  - **Exploder**: Medium speed, explodes on death/contact
  - **Boss**: Very slow, very high HP, special attacks
- [ ] 6.5 Add visual indicators
  - Enemy spawn warning (red circles)
  - Danger zones
  - Safe zone (shop area)

### Phase 7: Weapons & Items System ✓
- [ ] 7.1 Define weapon stats (damage, fire rate, ammo capacity, reload time)
- [ ] 7.2 Implement weapon switching
- [ ] 7.3 Implement ammo system (if using limited ammo)
- [ ] 7.4 Create shop items
  - Weapons (pistol $100, shotgun $300, rifle $500, sniper $800)
  - Health pack ($50 - restore 50 HP)
  - Armor ($200 - extra protection layer)
  - Speed boost ($150 - temporary speed increase)
  - Damage boost ($250 - temporary damage increase)
  - Ammo box ($75 - refill ammo)
- [ ] 7.5 Implement item effects
  - Health restore
  - Temporary buffs
  - Ammo refill

### Phase 8: Balance & Polish ✓
- [ ] 8.1 Balance enemy health and damage
- [ ] 8.2 Balance weapon costs and damage
- [ ] 8.3 Balance wave difficulty progression
- [ ] 8.4 Balance money rewards
- [ ] 8.5 Add sound effects (shooting, enemy sounds, explosions)
- [ ] 8.6 Add particle effects (blood, explosions, muzzle flash)
- [ ] 8.7 Test multiplayer synchronization
- [ ] 8.8 Optimize network traffic (enemy updates at 10Hz)
- [ ] 8.9 Add screen shake for explosions/damage

---

## 🏗️ Detailed Specifications

### Enemy Types

| Enemy Type | Health | Speed | Damage | Money | Behavior |
|------------|--------|-------|--------|-------|----------|
| **Zombie** | 50 | 0.8 | 10 | $10 | Slow melee, walks toward nearest player |
| **Runner** | 30 | 2.0 | 15 | $15 | Fast melee, charges at players |
| **Tank** | 250 | 0.5 | 25 | $50 | Very slow, very high HP, heavy melee |
| **Exploder** | 40 | 1.2 | 50 (explosion) | $25 | Explodes on death or contact |
| **Boss** | 1000 | 0.4 | 40 | $200 | Huge HP, spawns minions, area attacks |

### Weapon System

| Weapon | Cost | Damage | Fire Rate | Ammo | Special |
|--------|------|--------|-----------|------|---------|
| **Pistol** | $100 (starting) | 15 | Fast (0.3s) | Unlimited | Default weapon |
| **Shotgun** | $300 | 50 | Slow (1.5s) | 24 | Spread shot, close range |
| **Assault Rifle** | $500 | 25 | Very Fast (0.15s) | 90 | Automatic fire |
| **Sniper** | $800 | 100 | Very Slow (2.0s) | 15 | Long range, piercing |
| **Rocket Launcher** | $1200 | 150 | Slow (3.0s) | 6 | Splash damage |

### Shop Items

| Item | Cost | Effect | Duration |
|------|------|--------|----------|
| **Health Pack** | $50 | Restore 50 HP | Instant |
| **Armor** | $200 | +50 armor points | Until depleted |
| **Speed Boost** | $150 | +50% move speed | 30 seconds |
| **Damage Boost** | $250 | +50% damage | 30 seconds |
| **Ammo Box** | $75 | Refill all ammo | Instant |

### Wave System
- **Total waves**: 20
- **Wave scaling**: 
  - Wave 1-5: Easy (Zombies only)
  - Wave 6-10: Medium (Zombies + Runners)
  - Wave 11-15: Hard (All types except Boss)
  - Wave 16-19: Very Hard (All types)
  - Wave 20: Final Boss wave
- **Enemy count**: Base count × wave multiplier
  - Wave 1: 10 enemies
  - Wave 5: 25 enemies
  - Wave 10: 50 enemies
  - Wave 15: 75 enemies
  - Wave 20: 100 enemies + 3 Bosses
- **Buy phase**: 30 seconds between waves
- **Starting money**: $500 per player
- **Wave completion bonus**: $100 × wave number

---

## 🎨 UI Layout

```
┌──────────────────────────────────────────────────────────────┐
│ Wave: 5/20   Players: 3/4   Money: $850   Next Wave: 25s    │
├──────────────────────────────────────────────────────────────┤
│                                                               │
│                    [GAME WORLD]                              │
│                  Players vs AI Enemies                       │
│                                                               │
│  ┌──────────────────┐                                       │
│  │  🛒 SHOP         │                                       │
│  │  ─────────────   │                                       │
│  │  Weapons:        │       [Players fighting]             │
│  │  [🔫] Pistol     │                                       │
│  │     $100         │       [Zombies approaching]          │
│  │  [💥] Shotgun    │                                       │
│  │     $300         │       [Runner charging]              │
│  │  [🔫] Rifle      │                                       │
│  │     $500         │                                       │
│  │  ─────────────   │                                       │
│  │  Items:          │                                       │
│  │  [❤️] Health      │                                       │
│  │     $50          │                                       │
│  │  [🛡️] Armor      │                                       │
│  │     $200         │                                       │
│  └──────────────────┘                                       │
│                                                               │
├──────────────────────────────────────────────────────────────┤
│  Ammo: 24/90  │  HP: ████░░░░░░ 40/100  │  Kills: 47      │
└──────────────────────────────────────────────────────────────┘
```

---

## 📡 Network Flow

### Match Start
1. Server: Send `headerMatchStart` with gameMode = HORDE_DEFENSE
2. Server: Initialize horde defense state, spawn points
3. Server: Give each player starting money ($500)
4. Clients: Switch to horde defense mode, show HUD
5. Server: Start buy phase countdown (30s)

### Buy Phase
1. Server: Broadcast `headerHordeStateUpdate` (state = BUYING_PHASE)
2. Clients: Display shop UI, countdown timer
3. Player: Click on item in shop
4. Client: Send `headerBuyItemRequest` (itemType, itemID)
5. Server: Validate (check money, apply effects)
6. Server: Send `headerBuyItemResponse` (success/failure)
7. Server: If success, send `headerPlayerMoneyUpdate`
8. Client: Update UI, close shop or show error

### Wave Active
1. Server: Countdown ends, send `headerWaveStart` (wave number, enemy counts)
2. Client: Show "Wave X Starting!" banner
3. Server: Start spawning enemies
4. Server: For each enemy, send `headerSpawnEnemy` (type, position, ID)
5. Clients: Create enemy entity, start rendering
6. Server: Update enemy AI (move toward players, attack)
7. Server: Send `headerEnemyUpdate` batched at 10Hz
8. Clients: Interpolate enemy movement
9. Player: Shoot enemies (use existing bullet system)
10. Server: Detect bullet hits on enemies
11. Server: Reduce enemy health, send `headerEnemyUpdate`
12. Server: If enemy dies, send `headerEnemyDeath` (killer, money reward)
13. Client: Play death animation, remove enemy
14. Server: Award money, send `headerPlayerMoneyUpdate`
15. Server: Check if player hit by enemy
16. Server: Reduce player health, send update
17. Server: If player dies, mark as dead, send death packet
18. Server: Check if all enemies dead → wave complete
19. Server: Send `headerWaveComplete` (stats, bonus money)
20. Client: Show wave complete banner, update money
21. Server: Start next buy phase or check victory

### Match End
1. Server: Detect condition
   - Victory: All waves completed
   - Defeat: All players dead
2. Server: Send `headerMatchEnd` with result and stats
3. Clients: Display victory/defeat screen
4. Clients: Show final statistics (waves survived, kills, MVP)

---

## 🔧 Implementation Order

### Step 1: Foundation (Day 1-2)
✅ Let's start here!
- Add `HORDE_DEFENSE` to GameMode enum
- Create enemy types and state enums
- Define basic enemy data structure
- Add packet headers to packet.h

### Step 2: Basic Server Logic (Day 3-4)
- Create HordeDefenseManager class skeleton
- Implement enemy spawning at fixed positions
- Implement basic enemy AI (move toward nearest player)
- Handle player shooting enemies (reuse bullet system)

### Step 3: Wave System (Day 5-6)
- Implement wave progression
- Add buy phase timer
- Implement money system
- Add wave complete detection

### Step 4: Shop System (Day 7-8)
- Create shop item definitions
- Implement buy validation
- Add weapon switching
- Apply item effects

### Step 5: Client Rendering (Day 9-10)
- Render AI enemies
- Enemy health bars
- Death animations
- HUD elements

### Step 6: UI & Polish (Day 11-14)
- Shop UI panel
- Wave banners
- End game screen
- Visual effects and sounds

---

## 🧪 Testing Checklist

### Functionality Tests
- [ ] Can build all tower types
- [ ] Towers attack enemies correctly
- [ ] Enemies follow path to base
- [ ] Money is awarded correctly
- [ ] Base takes damage correctly
- [ ] Waves progress correctly
- [ ] Victory condition works
- [ ] Defeat condition works

### Multiplayer Tests
- [ ] Multiple players can build simultaneously
- [ ] Towers sync correctly across clients
- [ ] Enemies sync correctly across clients
- [ ] Money syncs correctly per player
- [ ] All players see match end simultaneously

### Edge Cases
- [ ] Handle tower placement on invalid tiles
- [ ] Handle building with insufficient money
- [ ] Handle rapid tower placement requests
- [ ] Handle player disconnect during wave
- [ ] Handle network lag/packet loss

---

## 📝 Code Files to Modify/Create

### New Files
- `/include/gameLayer/TowerDefenseManager.h`
- `/src/gameLayer/TowerDefenseManager.cpp`
- `/include/gameLayer/Tower.h`
- `/src/gameLayer/Tower.cpp`
- `/include/gameLayer/Enemy.h`
- `/src/gameLayer/Enemy.cpp`
- `/include/gameLayer/TowerDefenseUI.h`
- `/src/gameLayer/TowerDefenseUI.cpp`

### Modified Files
- `/include/gameLayer/GameRoom.h` (add TOWER_DEFENSE to enum)
- `/include/gameLayer/packet.h` (add new packet types)
- `/src/gameLayer/packet.cpp` (implement packet handling)
- `/src/gameLayer/server.cpp` (integrate tower defense logic)
- `/src/gameLayer/client.cpp` (render towers/enemies, handle UI)
- `/include/common/Phisics.h` (add tower/enemy entities if needed)

---

## 🎮 Controls

### Mouse Controls
- **Left Click**: Select tower / Place tower / Select existing tower
- **Right Click**: Cancel tower placement
- **Scroll Wheel**: Zoom in/out (optional)

### Keyboard Shortcuts
- **1-4**: Quick select tower type (1=Arrow, 2=Cannon, 3=Magic, 4=Slow)
- **U**: Upgrade selected tower
- **S**: Sell selected tower
- **Space**: Start next wave early (bonus money)
- **ESC**: Open pause menu / Cancel tower selection

---

## 💡 Future Enhancements

### Potential Features (Post-MVP)
- [ ] More tower types (Laser, Tesla, Freeze)
- [ ] More enemy types (Flying, Armored, Regenerating)
- [ ] Multiple paths/lanes
- [ ] Tower abilities (activate special powers)
- [ ] Player abilities (air strike, freeze all, etc.)
- [ ] Difficulty levels (Easy, Normal, Hard, Insane)
- [ ] Endless mode
- [ ] Leaderboards for longest survival
- [ ] Custom wave editor
- [ ] Different maps with unique layouts

---

## ✅ Ready to Implement!

Would you like me to start with:
1. **Phase 1**: Data structures and enums?
2. **Phase 2**: Network packets?
3. Or jump to a specific part?

Let me know and I'll implement it step by step!

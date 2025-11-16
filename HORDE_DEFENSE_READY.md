# Horde Defense Mode - Ready to Implement! 🎮

## 📋 Summary

The **Horde Defense** game mode plan is complete and ready for implementation. This is a cooperative PvE mode where players fight waves of AI enemies together.

---

## 🎯 Key Features

### Gameplay
- ✅ **Cooperative PvE**: Players vs AI enemies (not PvP)
- ✅ **Wave-Based Survival**: 20 waves of increasing difficulty
- ✅ **Direct Combat**: Players fight enemies themselves (NO towers)
- ✅ **Money System**: Earn cash from kills, spend on weapons/items
- ✅ **Shop System**: Buy weapons, health, armor, boosts between waves
- ✅ **Respawn System**: Dead players respawn at start of next wave
- ✅ **Victory**: Survive all 20 waves
- ✅ **Defeat**: All players die simultaneously

### Enemy Types (5 Types)
| Type | Health | Speed | Behavior |
|------|--------|-------|----------|
| **Zombie** | 50 | Slow | Basic melee enemy |
| **Runner** | 30 | Fast | Charges at players |
| **Tank** | 250 | Very Slow | High HP, heavy damage |
| **Exploder** | 40 | Medium | Explodes on death/contact |
| **Boss** | 1000 | Slow | Special attacks, spawns minions |

### Upgrade System (Permanent Stat Upgrades)
| Upgrade | Cost | Effect | Max Level |
|---------|------|--------|-----------|
| **Damage Upgrade** | $200 | +25% bullet damage | Level 5 |
| **Fire Rate Upgrade** | $250 | +20% faster shooting | Level 5 |
| **Health Upgrade** | $150 | +20 max HP | Level 5 |
| **Speed Upgrade** | $200 | +15% move speed | Level 5 |
| **Bullet Speed Upgrade** | $180 | +30% bullet velocity | Level 5 |

### Shop Items (Consumables & Temporary Buffs)
| Item | Cost | Effect | Duration |
|------|------|--------|----------|
| **Health Pack** | $50 | Restore 50 HP | Instant |
| **Max Health Boost** | $100 | +50 temporary max HP | Until death |
| **Shield** | $200 | Absorb 100 damage | Until depleted |
| **Speed Boost** | $150 | +50% move speed | 30 seconds |
| **Damage Amplifier** | $250 | +100% damage | 20 seconds |
| **Invincibility** | $500 | Cannot take damage | 5 seconds |
| **Multi-Shot** | $300 | Shoot 3 bullets at once | 30 seconds |

---

## 📊 Implementation Phases

### ✅ Phase 1: Data Structures & Enums (Foundation)
**Status**: IN PROGRESS ⚙️  
**Time**: 1-2 days  
**Tasks**:
- [x] Add `HORDE_DEFENSE` to GameMode enum
- [ ] Create enemy type enums (ZOMBIE, RUNNER, TANK, EXPLODER, BOSS)
- [ ] Create state enums (WAITING, BUYING_PHASE, WAVE_ACTIVE, etc.)
- [ ] Create upgrade type enums (DAMAGE, FIRE_RATE, HEALTH, SPEED, BULLET_SPEED)
- [ ] Create shop item type enums (HEALTH_PACK, SHIELD, SPEED_BOOST, etc.)
- [ ] Define enemy data structure (health, speed, damage, behavior)
- [ ] Define upgrade stats structure (cost per level, effect multiplier)
- [ ] Define shop item structure (cost, effect, duration)
- [ ] Define wave configuration structure
- [ ] Add player upgrade levels to Entity structure

### 🔲 Phase 2: Network Packets
**Status**: Next after Phase 1  
**Time**: 1-2 days  
**Tasks**:
- Add packet headers for horde defense
- Create packet data structures
- Implement serialization

### 🔲 Phase 3: Server-Side Logic
**Status**: After Phase 2  
**Time**: 3-4 days  
**Tasks**:
- Create HordeDefenseManager class
- Implement enemy spawning system
- Implement AI behavior
- Implement money/shop system
- Implement wave progression
- Implement respawn system

### 🔲 Phase 4: Client-Side Logic
**Status**: After Phase 3  
**Time**: 2-3 days  
**Tasks**:
- Enemy rendering
- Movement interpolation
- Shop UI
- Visual effects

### 🔲 Phase 5: UI Elements
**Status**: After Phase 4  
**Time**: 2-3 days  
**Tasks**:
- HUD (wave counter, money, timer)
- Shop panel
- Wave start/complete banners
- End game screen

### 🔲 Phase 6: AI & Pathfinding
**Status**: After Phase 3-4  
**Time**: 2-3 days  
**Tasks**:
- Enemy spawn points
- Pathfinding to players
- Attack logic
- Different behaviors per enemy type

### 🔲 Phase 7: Upgrades & Items System ✓
**Status**: After Phase 3-4  
**Time**: 2-3 days  
**Tasks**:
- Implement permanent upgrade system
- Track upgrade levels per player
- Apply upgrade multipliers to player stats
- Implement temporary buff system
- Item effects (health restore, shields, damage boost, etc.)
- Buff timers and expiration

### 🔲 Phase 8: Balance & Polish
**Status**: Final phase  
**Time**: 2-3 days  
**Tasks**:
- Balance testing
- Sound effects
- Particle effects
- Network optimization

---

## 🏗️ Architecture Overview

### Game Flow
```
Match Start
    ↓
Give Starting Money ($500)
    ↓
Buy Phase (30s) ← ─ ─ ─ ─ ─ ─ ─ ┐
    ↓                           │
Shop Opens                      │
    ↓                           │
Wave Starts                     │
    ↓                           │
Spawn Enemies                   │
    ↓                           │
Players Fight                   │
    ↓                           │
All Enemies Dead?               │
    ↓                           │
Wave Complete                   │
    ↓                           │
Award Bonus Money               │
    ↓                           │
More Waves? ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
    ↓ (No)
Victory!
```

### Victory/Defeat Conditions
- **Victory**: All 20 waves completed
- **Defeat**: All players dead at the same time
- **Respawn**: Dead players respawn at start of next wave (during buy phase)

### Key Differences from Deathmatch
| Feature | Deathmatch | Horde Defense |
|---------|------------|---------------|
| **Objective** | Kill other players | Survive waves |
| **Enemies** | Other players | AI enemies |
| **Respawn** | Instant | Next wave |
| **Money** | N/A | Earn from kills |
| **Shop** | N/A | Buy weapons/items |
| **Cooperation** | FFA | Team-based |

---

## 🎮 Controls (Proposed)

### Combat
- **Mouse**: Aim and shoot (same as deathmatch - basic shooting)
- **WASD**: Move player (same as deathmatch)
- **No weapon switching** - just shoot!

### Shop (during buy phase)
- **B** or **Tab**: Open/close shop
- **Click**: Buy upgrades/items
- **ESC**: Close shop

### Other
- **ESC**: Pause menu
- **Tab**: Scoreboard

---

## 📝 Files to Create/Modify

### New Files to Create
```
/include/gameLayer/HordeDefenseManager.h
/src/gameLayer/HordeDefenseManager.cpp
/include/gameLayer/Enemy.h
/src/gameLayer/Enemy.cpp
/include/gameLayer/HordeDefenseUI.h
/src/gameLayer/HordeDefenseUI.cpp
```

### Existing Files to Modify
```
/include/gameLayer/GameRoom.h        (add HORDE_DEFENSE enum)
/include/gameLayer/packet.h          (add new packet types)
/src/gameLayer/packet.cpp            (implement packet handling)
/src/gameLayer/server.cpp            (integrate horde defense logic)
/src/gameLayer/client.cpp            (render enemies, handle UI)
/include/common/Phisics.h            (add enemy entity if needed)
```

---

## ✅ Current Status

### Completed ✓
- [x] Game design document
- [x] Implementation plan with 8 phases
- [x] Enemy type specifications
- [x] Weapon system specifications
- [x] Shop item specifications
- [x] Wave progression design
- [x] Network packet design
- [x] UI layout mockups

### Ready to Implement ✓
- [x] Full specification ready
- [x] Clear implementation phases
- [x] Step-by-step checklist
- [x] All features designed

---

## 🚀 Next Steps

### Option 1: Start Phase 1 (RECOMMENDED)
Begin with data structures and enums:
1. Add `HORDE_DEFENSE` to GameMode enum
2. Create enemy type enums
3. Create state enums
4. Define enemy data structures
5. Define weapon structures

**Estimated Time**: 1-2 hours  
**Risk Level**: Low  
**Dependencies**: None

### Option 2: Review and Adjust
Review the plan, make adjustments, then start implementation.

### Option 3: Prototype
Create a quick prototype to test core mechanics before full implementation.

---

## 💬 Ready to Begin!

The plan is comprehensive and ready. Let me know if you want to:

1. **Start Phase 1 immediately** ✅ (Data structures/enums)
2. **Review/adjust the plan first** 📝
3. **Ask questions about any phase** ❓
4. **Jump to a specific phase** 🎯

Just say the word and we'll get started! 🚀

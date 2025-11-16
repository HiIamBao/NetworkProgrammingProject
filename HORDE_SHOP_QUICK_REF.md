# Horde Defense Shop - Quick Reference

## Shop Controls

### Opening/Closing
- **B Key**: Toggle shop menu (only during buy phase)
- **ESC**: Exit match (closes shop if open)

### Navigation
- **1 Key / LBumper**: Upgrades tab
- **2 Key / RBumper**: Items tab
- **W / Up**: Move selection up
- **S / Down**: Move selection down
- **Space / E / A Button**: Purchase selected item

---

## Upgrades (Permanent)

| Upgrade | Base Cost | Effect | Max Level |
|---------|-----------|--------|-----------|
| **Damage** | $200 | +25% bullet damage/level | 5 |
| **Fire Rate** | $250 | +20% faster shooting/level | 5 |
| **Health** | $150 | +20 max HP/level | 5 |
| **Speed** | $200 | +15% movement speed/level | 5 |
| **Bullet Speed** | $180 | +30% bullet velocity/level | 5 |

**Cost Scaling**: Each level costs more (Level 1 = base, Level 5 = base × 3)

---

## Shop Items (Consumable/Temporary)

| Item | Cost | Effect | Duration |
|------|------|--------|----------|
| **Health Pack** | $50 | Restore 50 HP | Instant |
| **Max Health Boost** | $100 | +50 max HP | Until death |
| **Shield** | $200 | Absorb 100 damage | Until depleted |
| **Speed Boost** | $150 | +50% movement speed | 30 seconds |
| **Damage Amplifier** | $250 | +100% damage | 20 seconds |
| **Invincibility** | $500 | Immune to damage | 5 seconds |
| **Multi-Shot** | $300 | Shoot 3 bullets | 30 seconds |

---

## Money Sources

- **Enemy Kills**: $10-$200 depending on type
- **Wave Completion**: $100 × wave number
- **Starting Money**: $500

---

## Strategy Tips

### Early Game (Waves 1-5)
- Focus on **Health** and **Damage** upgrades
- Save money for harder waves
- Buy Health Packs only when needed

### Mid Game (Waves 6-15)
- Prioritize **Fire Rate** and **Speed**
- Buy **Shield** before tough waves
- Use **Damage Amplifier** on tank enemies

### Late Game (Waves 16-20)
- Max out all key upgrades
- Stock up on **Invincibility** for boss waves
- Combine **Multi-Shot** + **Damage Amplifier** for max DPS

### Boss Waves
- Wave 16-19: 1 boss + many enemies
- Wave 20: 3 bosses + huge army
- Recommended: Max upgrades + Invincibility + Damage Amplifier

---

## Visual Indicators

- **Green Text**: You can afford this
- **Red Text**: Not enough money
- **Yellow Highlight**: Currently selected
- **Gray Text**: Max level reached
- **Purple Background**: Selection highlight

---

## Network Behavior

- Purchase requests sent to server
- Server validates (money check, level check)
- Response shows success/failure
- Money updates instantly on success
- Stats broadcast to all players

---

## Phase Status

- ✅ Phase 5A: Packet Handling
- ✅ Phase 5B: Enemy Rendering
- ✅ Phase 5C: HUD & Notifications
- ✅ **Phase 5D: Shop UI** ← YOU ARE HERE
- ⏳ Phase 5E: Bullet-Enemy Collision
- ⏳ Phase 5F: Visual Polish

---

**Last Updated**: November 7, 2025

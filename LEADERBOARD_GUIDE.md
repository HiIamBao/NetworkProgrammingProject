# Leaderboard System & Customization Guide

This document explains the architecture of the networked leaderboard and provides a step-by-step guide on how to modify the statistics displayed for each game mode.

## Architecture Overview

The system uses a **Server-Authoritative** model with **Client-Side Prediction** (Hybrid Approach) to ensure data accuracy while maintaining responsiveness.

1.  **Data Source (`Phisics.h`)**: Player statistics (Kills, Damage, Waves Survived) are stored in the `phisics::Entity` struct.
2.  **Server (`server.cpp`)**: 
    *   Runs a 1Hz loop (every 1.0s).
    *   Sorts all connected players based on the current `GameMode`.
    *   Broadcasts a `headerUpdateLeaderBoard` packet containing the Top 5 players and their relevant scores.
3.  **Client (`client.cpp`)**:
    *   Receives the packet and stores it in `activeLeaderboard`.
    *   **Rendering**: Prioritizes displaying the packet data. If no packet has arrived yet (e.g., just joined), it falls back to sorting the local entity list to ensure the leaderboard is never empty.

---

## How to Change the Displayed Statistic

To display a different statistic (e.g., "Headshots" or "Money"), follow these three steps:

### Step 1: Define the Data
Ensure the statistic exists in the player's data structure.

**File:** `include/common/Phisics.h`
Find the `struct Entity` definition.

```cpp
struct Entity
{
    // ... existing fields ...
    int kills = 0;
    int wavesSurvived = 0;
    
    // [NEW] Add your new statistic here
    int myNewStat = 0; 
};
```

### Step 2: Update Server Logic (Sorting & Broadcasting)
Tell the server how to sort the players and which value to send.

**File:** `src/gameLayer/server.cpp`
Search for `// LEADERBOARD BROADCAST` (approx. line 1030).

**A. Modify Sorting:**
Update the `sortFunc` lambda to use your new stat for the specific game mode.

```cpp
auto sortFunc = [instance](const auto& a, const auto& b) {
    if (instance->gameMode == GameMode::MY_CUSTOM_MODE) {
        // Sort by New Stat (Descending)
        return a.second->myNewStat > b.second->myNewStat;
    }
    // ... existing logic ...
};
```

**B. Populate Packet:**
Update the loop where `lbData.entries[i].value` is assigned.

```cpp
if (instance->gameMode == GameMode::MY_CUSTOM_MODE) {
    lbData.entries[i].value = p.second->myNewStat;
}
```

### Step 3: Update Client Logic (Rendering)
Tell the client how to label the column and what value to show if using the local fallback.

**File:** `src/gameLayer/client.cpp`
Search for `// LEADERBOARD (Universal for all modes)` (approx. line 2026).

**A. Update Label:**
```cpp
if (activeLeaderboard.gameMode == (int)GameMode::MY_CUSTOM_MODE) metricLabel = "My Stat";
```

**B. Update Fallback Logic** (Optional but recommended)
Update the local sorting logic inside the `else` block (when `activeLeaderboard.count == 0`) to match the server's sorting. This ensures the leaderboard looks correct immediately upon joining.

```cpp
if (currentGameMode == GameMode::MY_CUSTOM_MODE) {
    metricLabel = "My Stat";
    // ... update sort lambda similarly to server ...
}
```

## Example: Current Implementation

| Game Mode | Metric Label | Data Source (`Entity`) | Sorting Logic |
| :--- | :--- | :--- | :--- |
| **Deathmatch** | "Kills" | `.kills` | Kills (Desc) -> Deaths (Asc) |
| **Horde Defense**| "Wave" | `.wavesSurvived` | Waves (Desc) -> Damage (Desc) |
| **Boss Fight** | "Score" | `.totalDamageDealt`| Damage (Desc) |

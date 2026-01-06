#pragma once
#include<cstdint>
#include<enet/enet.h>

struct Packet
{
	int32_t header = 0;
	int32_t cid = 0;
	char *getData()
	{
		return (char *)((&cid) + 1);
	}
};

enum
{
	headerNone = 0,
	
	// Authentication packets
	headerRegisterRequest,      // Client -> Server: username, password, email
	headerRegisterResponse,     // Server -> Client: success/failure
	headerLoginRequest,         // Client -> Server: username, password
	headerLoginResponse,        // Server -> Client: session token or error
	headerLogoutRequest,        // Client -> Server: session token
	headerLogoutResponse,       // Server -> Client: success
	
	// Existing game packets
	headerReceiveCIDAndData,
	headerAnounceConnection,
	headerUpdateConnection,
	headerAnounceDisconnect,
	headerSendBullet,
	headerRegisterHit,			//contains pid of hit player, clients will recieve cid directly, no data associated
	headerSpawnItem,			//contains itemData
	headerPickupItem,			//contains itemId, when recieved by client contains full item data
	
	// Account info packets
	headerRequestAccountInfo,   // Client -> Server: session token
	headerAccountInfo,          // Server -> Client: account stats
	headerRequestLeaderboard,   // Client -> Server
	headerLeaderboard,          // Server -> Client: top players list
	
	// Room system packets
	headerCreateRoomRequest,    // Client -> Server: room creation data
	headerCreateRoomResponse,   // Server -> Client: room ID or error
	headerJoinRoomRequest,      // Client -> Server: room ID, password
	headerJoinRoomResponse,     // Server -> Client: success/failure
	headerLeaveRoomRequest,     // Client -> Server
	headerLeaveRoomResponse,    // Server -> Client: success
	headerGetRoomListRequest,   // Client -> Server
	headerGetRoomListResponse,  // Server -> Client: room list
	headerGetRoomInfoRequest,   // Client -> Server: room ID
	headerGetRoomInfoResponse,  // Server -> Client: detailed room info
	headerStartGameRequest,     // Client (host) -> Server
	headerStartGameResponse,    // Server -> Client: success/failure
	headerSetReadyRequest,      // Client -> Server: ready status
	headerSetReadyResponse,     // Server -> Client: success
	headerRoomPlayerJoined,     // Server -> All in room: player joined
	headerRoomPlayerLeft,       // Server -> All in room: player left
	headerRoomStatusChanged,    // Server -> All in room: room status changed
	headerRoomPlayerReadyChanged, // Server -> All in room: player ready status
	
	// Game Mode packets
	headerGameModeUpdate,       // Server -> Client: current game mode state
	headerMatchStart,           // Server -> All: match has started
	headerMatchEnd,             // Server -> All: match ended with results
	headerPlayerKill,           // Server -> All: player got a kill
	headerPlayerDeath,          // Server -> All: player died
	headerScoreUpdate,          // Server -> All: score/stats update
	
	// Tower Defense packets
	headerTowerDefenseStateUpdate,  // Server -> All: game state update (wave, timer, base health)
	headerBuildTowerRequest,        // Client -> Server: request to build tower
	headerBuildTowerResponse,       // Server -> Client: success/failure with reason
	headerTowerPlaced,              // Server -> All: tower was placed successfully
	headerUpgradeTowerRequest,      // Client -> Server: request to upgrade tower
	headerUpgradeTowerResponse,     // Server -> Client: success/failure
	headerTowerUpgraded,            // Server -> All: tower was upgraded
	headerSellTowerRequest,         // Client -> Server: request to sell tower
	headerTowerSold,                // Server -> All: tower was sold
	headerSpawnEnemy,               // Server -> All: enemy spawned
	headerEnemyUpdate,              // Server -> All: enemy position/health update (batched)
	headerEnemyDeath,               // Server -> All: enemy died
	headerEnemyReachedBase,         // Server -> All: enemy reached base, took damage
	headerBaseHealthUpdate,         // Server -> All: base health changed
	headerWaveStart,                // Server -> All: wave started
	headerWaveComplete,             // Server -> All: wave completed
	headerPlayerMoneyUpdate,        // Server -> Client: player money changed
	headerTowerAttack,              // Server -> All: tower fired (for visual effects)
	headerStartWaveEarly,           // Client -> Server: request to start wave early
	
	// Horde Defense packets (New game mode)
	headerHordeStateUpdate,         // Server -> All: game state (wave, timer, state)
	headerHordeSpawnEnemy,          // Server -> All: enemy spawned
	headerHordeEnemyUpdate,         // Server -> All: enemy position/health (batched, 10Hz)
	headerHordeEnemyDeath,          // Server -> All: enemy died, award money
	headerHordeWaveStart,           // Server -> All: wave started
	headerHordeWaveComplete,        // Server -> All: wave completed, bonus money
	headerHordeBuyUpgrade,          // Client -> Server: buy permanent upgrade
	headerHordeBuyUpgradeResponse,  // Server -> Client: upgrade success/failure
	headerHordeBuyItem,             // Client -> Server: buy shop item
	headerHordeBuyItemResponse,     // Server -> Client: item purchase result
	headerHordePlayerMoneyUpdate,   // Server -> Client: money changed
	headerHordePlayerStatsUpdate,   // Server -> All: player upgrade levels/buffs
	headerHordePlayerRespawn,       // Server -> All: player respawned
	headerHordeMatchEnd,            // Server -> All: match ended (victory/defeat)
	headerHordeBulletHitEnemy,      // Client -> Server: bullet hit enemy (for damage)
	headerHordeEnemyAttack,         // Server -> All: enemy attacks player (deal damage)
	headerHordeDamageUpdate,        // Server -> All: lightweight damage leaderboard update (batched)
	
	// Boss Fight mode packets
	headerBossFightStateUpdate,     // Server -> Client: Game state update
	headerBossFightBossSpawn,       // Server -> Client: Boss spawn notification
	headerBossFightBossUpdate,      // Server -> Client: Boss position/health update (10Hz)
	headerBossFightBossAttack,      // Server -> Client: Boss attack notification
	headerBossFightBossDeath,       // Server -> Client: Boss defeated
	headerBossFightMinionSpawn,     // Server -> Client: Minion spawn
	headerBossFightMinionUpdate,    // Server -> Client: Minion updates (10Hz)
	headerBossFightMinionDeath,     // Server -> Client: Minion death
	headerBossFightPlayerRespawn,   // Server -> Client: Player respawn
	headerBossFightMatchEnd,        // Server -> Client: Match end (victory/defeat)
	headerBossFightPlayerDamage,
	headerBossFightStartRequest,    // Client (host) -> Server: Request to start boss fight match
    gameEndHeader,      // Server -> Client: Player took damage from boss
	
	// Boss Fight DEBUG packets
	headerBossFightDebugRespawnBoss, // Client -> Server: Request to respawn boss at player position (DEBUG)
};

constexpr int SERVER_CHANNELS = 2;

void sendPacket(ENetPeer *to, Packet p, const char *data, size_t size, bool reliable, int channel);
char *parsePacket(ENetEvent &event, Packet &p, size_t &dataSize);

// ============================================================================
// HORDE DEFENSE - Helper Functions
// ============================================================================

// Send Horde Defense state update to all players
void sendHordeStateUpdate(ENetPeer* peer, const struct HordeStateUpdateData& data, bool reliable = true);

// Send enemy spawn notification
void sendHordeEnemySpawn(ENetPeer* peer, const struct HordeEnemySpawnData& data, bool reliable = true);

// Send batched enemy updates (unreliable, 10Hz)
void sendHordeEnemyUpdate(ENetPeer* peer, const struct HordeEnemyUpdateData* enemies, int count, bool reliable = false);

// Send enemy death notification
void sendHordeEnemyDeath(ENetPeer* peer, const struct HordeEnemyDeathData& data, bool reliable = true);

// Send wave start notification
void sendHordeWaveStart(ENetPeer* peer, const struct HordeWaveStartData& data, bool reliable = true);

// Send wave complete notification
void sendHordeWaveComplete(ENetPeer* peer, const struct HordeWaveCompleteData& data, bool reliable = true);

// Send player money update
void sendHordePlayerMoney(ENetPeer* peer, const struct HordePlayerMoneyUpdate& data, bool reliable = true);

// Send player stats update (upgrades and buffs)
void sendHordePlayerStats(ENetPeer* peer, const struct HordePlayerStatsUpdate& data, bool reliable = true);

// Send player respawn notification
void sendHordePlayerRespawn(ENetPeer* peer, const struct HordePlayerRespawnData& data, bool reliable = true);

// Send match end (victory/defeat)
void sendHordeMatchEnd(ENetPeer* peer, const struct HordeMatchEndData& data, bool reliable = true);

// Response helpers (server -> client)
void sendHordeBuyUpgradeResponse(ENetPeer* peer, const struct HordeBuyUpgradeResponse& data, bool reliable = true);
void sendHordeBuyItemResponse(ENetPeer* peer, const struct HordeBuyItemResponse& data, bool reliable = true);

// Room-related data structures
struct CreateRoomData {
    char roomName[32];
    char password[32];
    int maxPlayers;
    int gameMode;      // GameMode enum as int
    int mapId;
};

struct CreateRoomResponse {
    bool success;
    int roomId;
    char message[64];
};

struct JoinRoomData {
    int roomId;
    char password[32];
};

struct JoinRoomResponse {
    bool success;
    int roomId;
    char message[64];
};

struct RoomInfoData {
    int roomId;
    char roomName[32];
    char hostUsername[32];
    int currentPlayers;
    int maxPlayers;
    int gameMode;
    int mapId;
    bool hasPassword;
    int status;  // RoomStatus as int
};

struct RoomListResponse {
    int roomCount;
    // Followed by roomCount * RoomInfoData
};

struct PlayerInRoomData {
    char username[32];
    bool isReady;
    int team;  // -1 for no team
};

struct DetailedRoomInfo {
    RoomInfoData info;
    int playerCount;
    // Followed by playerCount * PlayerInRoomData
};

struct SetReadyData {
    bool ready;
};

struct PlayerJoinedData {
    char username[32];
};

struct PlayerLeftData {
    char username[32];
};

struct RoomStatusChangedData {
    int newStatus;  // RoomStatus as int
};

struct PlayerReadyChangedData {
    char username[32];
    bool isReady;
};

// Game Mode data structures
// Note: GameMode enum class is defined in GameRoom.h
// DEATHMATCH = 0 (Free for all), TEAM_BATTLE = 1, COOPERATIVE = 2

enum class MatchState {
    MATCH_WAITING = 0,
    MATCH_IN_PROGRESS = 1,
    MATCH_ENDED = 2,
};

struct PlayerScore {
    int32_t cid;
    char playerName[32];
    int kills;
    int deaths;
    int score;  // Kills - Deaths
};

struct MatchStartData {
    int gameMode;
    int matchDuration;  // in seconds, 0 = infinite
    int scoreLimit;     // 0 = no limit
    int mapId;          // Map selection (0=default, 1=industrial, 2=warehouse, 3=boss arena)
};

struct MatchEndData {
    int32_t winnerCid;
    char winnerName[32];
    int winnerKills;
    int winnerDeaths;
    int totalPlayers;
    // Followed by array of PlayerScore for all players
};

struct PlayerKillData {
    int32_t killerCid;
    int32_t victimCid;
    char killerName[32];
    char victimName[32];
};

struct ScoreUpdateData {
    int32_t cid;
    int kills;
    int deaths;
};

// ============================================================================
// HORDE DEFENSE MODE - Network Data Structures
// ============================================================================

struct HordeStateUpdateData {
    int currentWave;           // Current wave number (1-20)
    int gameState;             // HordeDefenseState as int
    float timeRemaining;       // Time remaining for current phase
    int playersAlive;          // Number of players still alive
    int enemiesRemaining;      // Enemies left in current wave
};

struct HordeEnemySpawnData {
    int32_t enemyId;           // Unique enemy ID
    int enemyType;             // EnemyType as int
    float posX, posY;          // Spawn position
    float health;              // Current health
    float maxHealth;           // Maximum health
};

struct HordeEnemyUpdateData {
    int32_t enemyId;
    float posX, posY;
    float health;
    int32_t targetPlayerId;    // CID of target player (-1 = no target)
};

struct HordeEnemyDeathData {
    int32_t enemyId;
    int32_t killerCid;         // Player who killed the enemy
    int moneyReward;           // Money awarded to killer
    int enemyType;             // For animation/effects
    float posX, posY;          // Death position for effects
};

struct HordeWaveStartData {
    int waveNumber;
    int totalEnemies;          // Total enemies in this wave
    int zombieCount;
    int runnerCount;
    int tankCount;
    int exploderCount;
    int bossCount;
};

struct HordeWaveCompleteData {
    int waveNumber;
    int completionBonus;       // Bonus money for all players
    int totalKills;            // Total enemies killed this wave
    int32_t mvpPlayerId;       // Player with most kills this wave
};

struct HordeBuyUpgradeData {
    int upgradeType;           // UpgradeType as int
    int currentLevel;          // Current level before purchase
};

struct HordeBuyUpgradeResponse {
    bool success;
    int upgradeType;           // UpgradeType as int
    int newLevel;              // New level after purchase
    int newMoney;              // Player's money after purchase
    char message[64];          // Error message if failed
};

struct HordeBuyItemData {
    int itemType;              // ShopItemType as int
};

struct HordeBuyItemResponse {
    bool success;
    int itemType;              // ShopItemType as int
    int newMoney;              // Player's money after purchase
    float effectValue;         // Effect value applied
    float duration;            // Effect duration
    char message[64];          // Error message if failed
};

struct HordePlayerMoneyUpdate {
    int32_t cid;
    int newMoney;
    int changeAmount;          // Money gained/lost
    char reason[32];           // "Enemy Kill", "Wave Bonus", "Purchase", etc.
};

struct HordePlayerStatsUpdate {
    int32_t cid;
    // Upgrade levels
    int damageLevel;
    int fireRateLevel;
    int healthLevel;
    int speedLevel;
    int bulletSpeedLevel;
    // Active buffs (wave-based)
    int speedBoostWaves;
    int damageBoostWaves;
    int multiShotWaves;
    float shieldHealth;  // Kept for compatibility
};

struct HordePlayerRespawnData {
    int32_t cid;
    float posX, posY;          // Respawn position
};

struct HordeMatchEndData {
    bool victory;              // true = all waves completed, false = all players dead
    int finalWave;             // Last wave reached
    int totalKills;            // Total enemies killed
    int totalMoney;            // Total money earned
    int32_t mvpPlayerId;       // Player with most kills
    char mvpPlayerName[32];
    int mvpKills;
};

struct HordeBulletHitEnemyData {
    int32_t enemyId;           // ID of enemy that was hit
    int damage;                // Damage dealt (calculated by server based on upgrades)
};

struct HordeEnemyAttackData {
    int32_t enemyId;           // ID of enemy attacking
    int32_t targetCid;         // Player being attacked
    int damage;                // Damage dealt to player
    int enemyType;             // Type of enemy (for animation/effects)
};

// Lightweight damage update for leaderboard (batched to reduce network traffic)
struct HordeDamageUpdate {
    int32_t cid;               // Player ID
    int totalDamageDealt;      // Total damage dealt
    int enemiesKilled;         // Total enemies killed
};

// ============================================================================
// BOSS FIGHT MODE - Network Data Structures
// ============================================================================

struct BossFightStateUpdateData {
    int gameState;             // BossFightState as int
    float bossHealth;          // Current boss health
    float bossMaxHealth;       // Boss max health
    int bossPhase;             // BossPhase as int
    int playersAlive;          // Number of players alive
    float matchTime;           // Time elapsed since match start
};

struct BossFightBossSpawnData {
    int32_t bossId;
    int bossType;              // BossType as int
    float posX, posY;          // Spawn position
    float health;
    float maxHealth;
    float speed;
};

struct BossFightBossUpdateData {
    int32_t bossId;
    float posX, posY;
    float velX, velY;
    float health;
    int currentPhase;          // BossPhase as int
    int32_t targetPlayerId;    // Current target (-1 = no target)
};

struct BossFightBossAttackData {
    int32_t bossId;
    int attackType;            // BossAttackType as int
    float attackPosX, attackPosY; // Attack position (for AOE center)
    int32_t targetCid;         // Primary target CID (-1 for AOE)
    int damage;                // Base damage of attack
};

struct BossFightBossDeathData {
    int32_t bossId;
    int32_t lastHitPlayerCid;  // Player who landed killing blow
    float posX, posY;          // Death position for effects
};

struct BossFightMinionSpawnData {
    int32_t minionId;
    float posX, posY;
    float health;
    float maxHealth;
};

struct BossFightMinionUpdateData {
    int32_t minionId;
    float posX, posY;
    float health;
    int32_t targetPlayerId;
};

struct BossFightMinionDeathData {
    int32_t minionId;
    int32_t killerCid;
    float posX, posY;
};

struct BossFightPlayerRespawnData {
    int32_t cid;
    float posX, posY;
};

struct BossFightMatchEndData {
    bool victory;              // true = boss defeated, false = all players dead
    float matchDuration;       // Time to complete (or fail)
    int32_t mvpPlayerId;       // Most damage dealt
    char mvpPlayerName[32];
    int mvpDamage;             // Total damage dealt by MVP
    int totalPlayerDeaths;     // Total deaths across all players
};

struct BossFightPlayerDamageData {
    int32_t cid;               // Player who took damage
    int damage;                // Damage amount
    int attackType;            // BossAttackType as int
    float knockbackX, knockbackY; // Knockback vector
};

// DEBUG: Request to respawn boss at specific position
struct BossFightDebugRespawnBossData {
    float posX, posY;          // Position to spawn boss at
};
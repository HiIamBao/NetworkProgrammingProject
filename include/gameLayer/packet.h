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
};

constexpr int SERVER_CHANNELS = 2;

void sendPacket(ENetPeer *to, Packet p, const char *data, size_t size, bool reliable, int channel);
char *parsePacket(ENetEvent &event, Packet &p, size_t &dataSize);

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
#include "serverClient.h"
#include <platform/platformInput.h>
#include <Phisics.h>
#include <packet.h>
#include <unordered_map>
#include "Ui.h"
#include <glui/glui.h>
#include <algorithm>
#include <string>
#include <iostream>
#include "GameRoom.h"
#include "HordeDefense.h"
#include "BossFight.h"
#include <map>
#include <cstring>
#include <AccountManager.h>
#include "AudioManager.h"

phisics::MapData map;

ENetPeer *server = {};
int32_t cid = {};
bool joined = false;
ENetHost *client;

#pragma region Player & Game State
std::unordered_map<int32_t, phisics::Entity> players;

static std::vector<phisics::Bullet> bullets;
static std::vector<phisics::Bullet> ownBullets;
static std::vector<phisics::Item> items;
static bool hasBatery = 0;

#pragma endregion

#pragma region Game Mode State
// Game mode state
static GameMode currentGameMode = GameMode::DEATHMATCH;
static MatchState currentMatchState = MatchState::MATCH_WAITING;
static bool matchEnded = false;
static char matchWinnerName[32] = {};
static int matchWinnerKills = 0;
static std::string lastKillMessage = "";
static float killMessageTimer = 0.0f;
// Tracks whether we've already received the initial Boss Fight mode announcement
static bool receivedInitialBossModeInfo = false;

// Account Manager for recording match results
static AccountManager* g_clientAccountManager = nullptr;

// Account UI for displaying match summary
#include "AccountUI.h"
extern AccountUI* g_accountUI;

void setClientAccountManager(AccountManager* accMgr) {
    g_clientAccountManager = accMgr;
}

// Networked Leaderboard State
static LeaderBoardUpdateData activeLeaderboard = {};

// Force disconnect state (for session control)
std::string g_forceDisconnectReason = "";
bool g_wasForceDisconnected = false;

#pragma endregion

#pragma region Horde Defense State
// Horde Defense client-side state
static std::map<int32_t, HordeDefense::Enemy> hordeEnemies;
static HordeDefense::HordeDefenseState hordeState = HordeDefense::HordeDefenseState::WAITING;
static int currentWave = 0;
static int totalWaves = 20;
static float phaseTimer = 0.0f;
static int playerMoney = 0;
static int enemiesAlive = 0;
static std::string waveNotification = "";
static float waveNotificationTimer = 0.0f;
static bool showShopUI = false;

// Shop UI state
static int selectedShopTab = 0;  // 0 = Upgrades, 1 = Items
static int selectedUpgradeIndex = 0;
static int selectedItemIndex = 0;
static std::string lastShopMessage = "";
static float shopMessageTimer = 0.0f;

// Player's current upgrade levels (for UI display)
static HordeDefense::PlayerUpgrades playerUpgrades;

#pragma endregion

#pragma region Boss Fight State
// Boss Fight client-side state
static BossFight::Boss clientBoss;
struct ClientBossBullet {
    glm::vec2 pos;
    glm::vec2 velocity;
    float lifetime;
};
static std::vector<ClientBossBullet> clientBossBullets;
static std::vector<ClientBossBullet> hordeBossBullets; // Horde Defense boss projectiles
static BossFight::BossFightState bossFightState = BossFight::BossFightState::WAITING;
static float bossAttackAnimTimer = 0.0f;
static BossFight::BossAttackType lastBossAttack = BossFight::BossAttackType::MELEE;
static std::string bossNotification = "";
static float bossNotificationTimer = 0.0f;
static glm::vec2 aoeAttackPos = glm::vec2(0, 0);
static float aoeAttackRadius = 0.0f;

#pragma endregion

glm::ivec2 spawnPositions[] =
{
	{5,5},
	{2,46},
	{44,44},
	{45,4}
};

glm::ivec2 getSpawnPosition()
{
	// Try to find a spawn position that doesn't collide with walls
	int numPositions = sizeof(spawnPositions) / sizeof(spawnPositions[0]);
	int startIndex = rand() % numPositions;
	
	for (int attempts = 0; attempts < numPositions; attempts++) {
		int index = (startIndex + attempts) % numPositions;
		glm::ivec2 pos = spawnPositions[index];
		
		// Check if the spawn position and surrounding area is not a wall
		// We check multiple tiles to ensure the player doesn't spawn partially in a wall
		bool isValid = true;
		for (int dx = 0; dx <= 1 && isValid; dx++) {
			for (int dy = 0; dy <= 1 && isValid; dy++) {
				if (map.get(pos.x + dx, pos.y + dy).isCollidable()) {
					isValid = false;
				}
			}
		}
		
		if (isValid) {
			return pos;
		}
	}
	
	// Fallback: return the first spawn position (shouldn't happen if spawn positions are set correctly)
	return spawnPositions[startIndex];
}

void resetClient()
{
	// Load appropriate map based on game mode
	const char* mapFile;
	if (currentGameMode == GameMode::BOSS_FIGHT) {
		mapFile = RESOURCES_PATH "bossFightArena.bin";
	} else if (currentGameMode == GameMode::HORDE_DEFENSE) {
		mapFile = RESOURCES_PATH "hordeDefense.bin";
	} else {
		mapFile = RESOURCES_PATH "mapData2.bin";
	}
	
	if (!map.load(mapFile))
	{
		return ;
	}

	players.clear();
	bullets.clear();
	ownBullets.clear();
	items.clear();
	hasBatery = false;
	
	// Reset game mode state (keep currentGameMode - it's set by server)
	// currentGameMode = GameMode::DEATHMATCH;  // Don't reset - preserve from server
	currentMatchState = MatchState::MATCH_WAITING;
	matchEnded = false;
	matchWinnerName[0] = '\0';
	matchWinnerKills = 0;
	lastKillMessage = "";
	killMessageTimer = 0.0f;
	// Allow Boss Fight START button to appear again on fresh connections
	receivedInitialBossModeInfo = false;
	
	// Reset Horde Defense state
	hordeEnemies.clear();
	hordeState = HordeDefense::HordeDefenseState::WAITING;
	currentWave = 0;
	totalWaves = 20;
	phaseTimer = 0.0f;
	playerMoney = 0;
	enemiesAlive = 0;
	waveNotification = "";
	waveNotificationTimer = 0.0f;
	showShopUI = false;
	
	// Reset shop UI state
	selectedShopTab = 0;
	selectedUpgradeIndex = 0;
	selectedItemIndex = 0;
	lastShopMessage = "";
	shopMessageTimer = 0.0f;
	playerUpgrades = HordeDefense::PlayerUpgrades();
	
	// Reset Boss Fight state
	clientBoss = BossFight::Boss();
	clientBoss.isAlive = false;
	bossFightState = BossFight::BossFightState::WAITING;
	bossAttackAnimTimer = 0.0f;
	lastBossAttack = BossFight::BossAttackType::MELEE;
	bossNotification = "";
	bossNotificationTimer = 0.0f;
	aoeAttackPos = glm::vec2(0, 0);
	aoeAttackRadius = 0.0f;
	
	// Reset leaderboard to prevent stale data from previous match
	activeLeaderboard = {};
	
	// Reset boss bullets for both modes
	clientBossBullets.clear();
	hordeBossBullets.clear();

	//todo add a struct here

	joined = false;
	client = nullptr;

	//todo
	//enet_host_destroy(server);
	server = {};
	cid = {};
}

void sendPlayerData(phisics::Entity &e, bool reliable)
{
	Packet p;
	p.cid = cid;
	p.header = headerUpdateConnection;
	sendPacket(server, p, (const char *)&e, sizeof(phisics::Entity), reliable, 0);
}

bool connectToServer(ENetHost *&client, ENetPeer *&server, int32_t &cid, std::string ip, char *playerName, int port)
{
	ENetAddress adress;
	ENetEvent event;

	if (ip.empty())
	{
		enet_address_set_host(&adress, "127.0.0.1");
	}
	else
	{
		enet_address_set_host(&adress, ip.c_str());
	}
	//enet_address_set_host(&adress, "95.76.249.14");
	//enet_address_set_host(&adress, "192.168.1.11");
	adress.port = port;  // Use the port parameter instead of hardcoded 7778

	//client, adress, channels, data to send rightAway
	server = enet_host_connect(client, &adress, SERVER_CHANNELS, 0);

	if (server == nullptr)
	{
		return false;
	}

	//see if we got events by server
	//client, event, ms to wait(0 means that we don't wait)
	if (enet_host_service(client, &event, 5000) > 0
		&& event.type == ENET_EVENT_TYPE_CONNECT)
	{
		//std::cout << "connected\n";
	}
	else
	{
		enet_peer_reset(server);
		return false;
	}


	if (enet_host_service(client, &event, 5000) > 0
		&& event.type == ENET_EVENT_TYPE_RECEIVE)
	{
		Packet p = {};
		size_t size;
		auto data = parsePacket(event, p, size);

		if (p.header != headerReceiveCIDAndData)
		{
			enet_peer_reset(server);
			return false;
		}

		cid = p.cid;

		glm::vec3 color = *(glm::vec3 *)data;
		auto e = phisics::Entity();
		e.pos = getSpawnPosition();
		e.lastPos = e.pos;
		e.color = color;
		memcpy(e.name, playerName, playerNameSize);
		players[cid] = e;

		sendPlayerData(e, true);

		//std::cout << "received cid: " << cid << "\n";
		enet_packet_destroy(event.packet);
		return true;
	}
	else
	{
		enet_peer_reset(server);
		return 0;
	}

	//std::cout << "fully connected\n";

	//name
	//{
	//	sendPacket(server, {headerClientSendName, cid}, userName.c_str(), userName.size() + 1);
	//}

	return true;
}

void msgLoop(ENetHost *client)
{
	
	ENetEvent event;
	if(enet_host_service(client, &event, 0) > 0)
	{
		switch (event.type)
		{
			case ENET_EVENT_TYPE_RECEIVE:
			{
				// std::cout << event.packet->dataLength << "\n";
				// std::cout << "recieved: " << event.packet->data << "\n";
				// std::cout << event.peer->data << "\n"; //recieved from
				// std::cout << event.peer->address.host << "\n"; //recieved from
				// std::cout << event.peer->address.port << "\n"; //recieved from
				// std::cout << event.channelID << "\n";
				Packet p = {};
				size_t size = {};
				auto data = parsePacket(event, p, size);

				if (p.header == headerAnounceConnection)
				{

					players[p.cid] = *(phisics::Entity*)data;


				}else if (p.header == headerUpdateLeaderBoard)
				{
					activeLeaderboard = *(LeaderBoardUpdateData*)data;
				}else if (p.header == headerUpdateConnection)
				{
					phisics::Entity updatedEntity = *(phisics::Entity *)data;
					
					// Log updates in Horde Defense mode
					if (currentGameMode == GameMode::HORDE_DEFENSE)
					{
						auto it = players.find(p.cid);
						if (it != players.end()) {
							// Log HP changes for our player
							if (p.cid == cid) {
								auto oldHP = it->second.life;
								auto oldMaxHP = it->second.maxLife;
								std::cout << "[ClientUpdate] Received entity update: HP " << oldHP << "->" << updatedEntity.life 
								          << ", MaxHP " << oldMaxHP << "->" << updatedEntity.maxLife << std::endl;
							}
							
							// Log damage changes for all players
							if (it->second.totalDamageDealt != updatedEntity.totalDamageDealt) {
								std::cout << "[ClientUpdate] Player " << p.cid << " (" << updatedEntity.name 
								          << ") damage: " << it->second.totalDamageDealt << "->" << updatedEntity.totalDamageDealt << std::endl;
							}
						}
					}
					
					players[p.cid] = updatedEntity;

				}else if (p.header == headerAnounceDisconnect)
				{
					auto find = players.find(p.cid);
					players.erase(find);
				}
				else if (p.header == headerForceDisconnect)
				{
					// Another client logged in with same account - we're being kicked
					auto disconnectData = *(ForceDisconnectData*)data;
					std::cout << "Force disconnect: " << disconnectData.reason << std::endl;
					
					// Signal to return to login screen
					// Set joined to false so the client knows to disconnect
					joined = false;
					
					// Store the disconnect reason for UI to display
					extern std::string g_forceDisconnectReason;
					extern bool g_wasForceDisconnected;
					g_forceDisconnectReason = std::string(disconnectData.reason);
					g_wasForceDisconnected = true;
					
					// Reset the server connection
					if (server) {
						enet_peer_reset(server);
						server = nullptr;
					}
				}else if (p.header == headerSendBullet)
				{
					bullets.push_back(*(phisics::Bullet *)data);
				}
				else if (p.header == headerRegisterHit)
				{
					auto find = players.find(p.cid);
					bool h = find->second.hit();

					if (h && find->first == cid)
					{
						find->second.life -= 1;

						if (find->second.life <= 0)
						{
							auto &p = find->second;
							p.pos = getSpawnPosition();
							p.lastPos = p.pos;
							p.life = p.maxLife;

							sendPlayerData(p, true);
						}

					}
				}
				else if (p.header == headerSpawnItem)
				{
					items.push_back(*(phisics::Item *)data);
				}
				else if (p.header == headerPickupItem)
				{
					auto item = *(phisics::Item*)data;
					auto f = std::find_if(items.begin(), items.end(), [item](phisics::Item &i) { return i.itemId == item.itemId; });

					if (f != items.end())
					{
						items.erase(f);
					}

					if (p.cid == cid)
					{
						auto find = players.find(p.cid);

						if (item.itemType == phisics::itemTypeHealth)
						{
							find->second.life = find->second.maxLife;  // Heal to max HP
						}
						else if (item.itemType == phisics::itemTypeBatery)
						{
							hasBatery = true;
						}

					}

				}
				else if (p.header == headerPlayerKill)
				{
					// Handle kill notification
					auto killData = *(PlayerKillData*)data;
					
					// Update local player stats
					auto killerIt = players.find(killData.killerCid);
					auto victimIt = players.find(killData.victimCid);
					
					if (killerIt != players.end())
					{
						killerIt->second.kills++;
					}
					
					if (victimIt != players.end())
					{
						victimIt->second.deaths++;
					}
					
					// Display kill message
					lastKillMessage = std::string(killData.killerName) + " eliminated " + std::string(killData.victimName);
					killMessageTimer = 3.0f;  // Display for 3 seconds
					
					std::cout << lastKillMessage << std::endl;
				}
				else if (p.header == headerMatchEnd)
				{
					// Match ended! (Supports all game modes)
					auto endData = *(MatchEndData*)data;
					
					matchEnded = true;
					currentMatchState = MatchState::MATCH_ENDED;
					strncpy(matchWinnerName, endData.winnerName, sizeof(matchWinnerName) - 1);
					matchWinnerKills = endData.winnerKills;
					
					// Determine game mode name
					const char* gameModeName = "Unknown";
					switch (endData.gameMode) {
						case GameMode::DEATHMATCH:
							gameModeName = "Deathmatch";
							break;
						case GameMode::HORDE_DEFENSE:
							gameModeName = "Horde Defense";
							break;
						case GameMode::BOSS_FIGHT:
							gameModeName = "Boss Fight";
							break;
					}
					
					// Display victory/defeat status with game mode
					std::cout << "========================================" << std::endl;
					std::cout << "  " << gameModeName << " - " << (endData.victory ? "VICTORY!" : "DEFEAT!") << std::endl;
					std::cout << "  MVP: " << matchWinnerName << " (" << matchWinnerKills 
					          << (endData.gameMode == GameMode::DEATHMATCH ? " kills)" : " damage)") << std::endl;
					std::cout << "========================================" << std::endl;
					
					// Collect all player scores for the scoreboard
					std::vector<PlayerScore> playerScores;
					for (int i = 0; i < endData.totalPlayers; i++)
					{
						playerScores.push_back(endData.scores[i]);
					}
					
					// Save ALL players' match stats to database
					if (g_clientAccountManager) {
						std::vector<MatchPlayerStats> matchStats;
						
						for (const auto& playerPair : players) {
							const auto& playerEntity = playerPair.second;
							MatchPlayerStats pStats;
							pStats.playerId = playerPair.first;
							pStats.playerName = std::string(playerEntity.name);
							pStats.kills = playerEntity.kills;
							pStats.roundsSurvived = 0;  // Not used in deathmatch
							pStats.damageDealt = 0;      // Not tracked in deathmatch
							matchStats.push_back(pStats);
						}
						
						if (!matchStats.empty()) {
							if (g_clientAccountManager->recordDeathmatchMatchEnd(matchStats)) {
								std::cout << "[Deathmatch] Recorded match stats for " << matchStats.size() << " players!" << std::endl;
							}
						}
					}
					
					// Transition to match summary screen
					extern AccountUI* g_accountUI;
					if (g_accountUI) {
						g_accountUI->setMatchSummary(matchWinnerName, matchWinnerKills, 
						                              endData.totalPlayers, playerScores, 	
						                              static_cast<int>(endData.gameMode), endData.victory);
						g_accountUI->setState(UIState::MATCH_SUMMARY);
						std::cout << "[Client] Transitioning to MATCH_SUMMARY screen" << std::endl;
					}
				}
				/// DUNG headerMatchEnd cho tat ca game mode

				// else if (p.header == headerHordeMatchEnd)
				// {
				// 	// Horde Defense match ended!
				// 	auto endData = *(HordeMatchEndData*)data;
				// 	matchEnded = true;
				// 	currentMatchState = MatchState::MATCH_ENDED;
					
				// 	// Set winner info (MVP in Horde Defense)
				// 	strncpy(matchWinnerName, endData.mvpPlayerName, sizeof(matchWinnerName) - 1);
				// 	matchWinnerName[sizeof(matchWinnerName) - 1] = '\0';
				// 	matchWinnerKills = endData.mvpKills;
					
				// 	// Set notification message
				// 	if (endData.victory)
				// 	{
				// 		waveNotification = "VICTORY! All waves completed!";
				// 	}
				// 	else
				// 	{
				// 		waveNotification = "DEFEATED! Wave " + std::to_string(endData.finalWave);
				// 	}
				// 	waveNotificationTimer = 10.0f;
					
				// 	std::cout << "[HordeDefense] Match ended - Victory: " << endData.victory 
				// 	          << ", Final Wave: " << endData.finalWave << std::endl;
					
				// 	// Collect all player scores for the scoreboard
				// 	std::vector<PlayerScore> playerScores;
				// 	for (const auto& playerPair : players) {
				// 		const auto& playerEntity = playerPair.second;
				// 		PlayerScore pScore;
				// 		pScore.cid = playerPair.first;
				// 		strncpy(pScore.playerName, playerEntity.name, sizeof(pScore.playerName) - 1);
				// 		pScore.playerName[sizeof(pScore.playerName) - 1] = '\0';
				// 		pScore.kills = playerEntity.kills;
				// 		pScore.deaths = 0;  // Not tracked in Horde Defense
				// 		pScore.score = playerEntity.totalDamageDealt;  // Use damage as score for Horde Defense
				// 		playerScores.push_back(pScore);
				// 	}
					
				// 	// Record match results to leaderboard
				// 	if (g_clientAccountManager)
				// 	{
				// 		std::vector<MatchPlayerStats> matchStats;
						
				// 		// Collect stats for all players in the match
				// 		for (const auto& [playerCid, playerEntity] : players)
				// 		{
				// 			MatchPlayerStats stats;
				// 			stats.playerId = playerCid;
				// 			stats.playerName = std::string(playerEntity.name);
				// 			stats.roundsSurvived = endData.finalWave;  // Use final wave reached
				// 			stats.damageDealt = playerEntity.totalDamageDealt;
				// 			stats.kills = playerEntity.kills;
				// 			matchStats.push_back(stats);
							
				// 			std::cout << "[HordeDefense] Recording stats for " << stats.playerName 
				// 			          << ": Wave " << stats.roundsSurvived 
				// 			          << ", Damage " << stats.damageDealt << std::endl;
				// 		}
						
				// 		// Record to database
				// 		if (g_clientAccountManager->recordHordeDefenseMatchEnd(matchStats))
				// 		{
				// 			std::cout << "[HordeDefense] Match results saved to leaderboard!" << std::endl;
				// 		}
				// 		else
				// 		{
				// 			std::cout << "[HordeDefense] Failed to save match results!" << std::endl;
				// 		}
				// 	}
					
				// 	// Transition to match summary screen
				// 	extern AccountUI* g_accountUI;
				// 	if (g_accountUI) {
				// 		g_accountUI->setMatchSummary(matchWinnerName, matchWinnerKills, 
				// 		                              (int)players.size(), playerScores, 
				// 		                              static_cast<int>(currentGameMode));
				// 		g_accountUI->setState(UIState::MATCH_SUMMARY);
				// 		std::cout << "[Client] Transitioning to MATCH_SUMMARY screen (Horde Defense)" << std::endl;
				// 	}
				// }
				else if (p.header == headerMatchStart)
				{
					// Match started OR game mode update (sent on connection)
					auto startData = *(MatchStartData*)data;
					GameMode newGameMode = static_cast<GameMode>(startData.gameMode);
					
					// Check if game mode changed - if so, we need to reload map
					bool gameModeChanged = (currentGameMode != newGameMode);
					currentGameMode = newGameMode;
					
					// Reset leaderboard when game mode changes to prevent stale data
					if (gameModeChanged) {
						activeLeaderboard = {};
					}
					
					if (currentGameMode == GameMode::BOSS_FIGHT) {
						clientBossBullets.clear();
						// Reset other boss state if needed
					}

					// Distinguish initial Boss Fight mode announcement from actual start
					bool wasWaiting = (currentMatchState == MatchState::MATCH_WAITING);
					bool initialBossModeAnnouncement = (
						newGameMode == GameMode::BOSS_FIGHT &&
						currentMatchState == MatchState::MATCH_WAITING &&
						bossFightState == BossFight::BossFightState::WAITING &&
						!receivedInitialBossModeInfo
					);

					if (initialBossModeAnnouncement)
					{
						// Keep state as WAITING so the host sees the START button
						receivedInitialBossModeInfo = true;
						matchEnded = false;
					}
					else
					{
						// Actual match start (or non-boss modes): move to IN_PROGRESS
						currentMatchState = MatchState::MATCH_IN_PROGRESS;
						matchEnded = false;
					}
					
					// Load correct map based on game mode (always reload if game mode changed)
					const char* mapFile;
					if (currentGameMode == GameMode::BOSS_FIGHT) {
						mapFile = RESOURCES_PATH "bossFightArena.bin";
					} else if (currentGameMode == GameMode::HORDE_DEFENSE) {
						mapFile = RESOURCES_PATH "hordeDefense.bin";
					} else {
						mapFile = RESOURCES_PATH "mapData2.bin";
					}
					
					// Reload map if game mode changed or if we're starting a match
					if (gameModeChanged || wasWaiting)
					{
						std::cout << "Game mode: " << static_cast<int>(currentGameMode) 
						          << (gameModeChanged ? " (changed)" : "") << std::endl;
						std::cout << "Loading map: " << mapFile << std::endl;
						
						map.cleanup();
						if (!map.load(mapFile)) {
							std::cout << "Failed to load map: " << mapFile << std::endl;
						} else {
							std::cout << "Map loaded successfully: " << mapFile << std::endl;
						}
					}
					
					// Initialize game mode-specific state
					if (wasWaiting || gameModeChanged)
					{
						if (currentGameMode == GameMode::DEATHMATCH)
						{
							std::cout << "Match started! Game mode: Free-for-All Deathmatch" << std::endl;
						}
						else if (currentGameMode == GameMode::HORDE_DEFENSE)
						{
							std::cout << "Match started! Game mode: Horde Defense" << std::endl;
							hordeEnemies.clear();
							hordeState = HordeDefense::HordeDefenseState::WAITING;
							currentWave = 0;
							playerMoney = 500;  // Starting money
						}
						else if (currentGameMode == GameMode::BOSS_FIGHT)
						{
							std::cout << "Match started! Game mode: Boss Fight" << std::endl;
							clientBoss = BossFight::Boss();
							clientBoss.isAlive = false;

							bossFightState = BossFight::BossFightState::WAITING;
						}
					}
				}
				// ========================================================================
				// HORDE DEFENSE PACKET HANDLERS
				// ========================================================================
				else if (p.header == headerHordeStateUpdate)
				{
					auto stateData = *(HordeStateUpdateData*)data;
					hordeState = static_cast<HordeDefense::HordeDefenseState>(stateData.gameState);
					currentWave = stateData.currentWave;
					phaseTimer = stateData.timeRemaining;
					enemiesAlive = stateData.enemiesRemaining;
					
					// Debug output
					//std::cout << "Horde State: Wave " << currentWave << "/" << totalWaves 
					//          << " Timer: " << phaseTimer << "s" << std::endl;
				}
				else if (p.header == headerHordeSpawnEnemy)
				{
					auto spawnData = *(HordeEnemySpawnData*)data;
					HordeDefense::Enemy enemy;
					enemy.id = spawnData.enemyId;
					enemy.type = static_cast<HordeDefense::EnemyType>(spawnData.enemyType);
					enemy.position = glm::vec2(spawnData.posX, spawnData.posY);
					enemy.health = spawnData.health;
					enemy.maxHealth = spawnData.maxHealth;
					enemy.speed = HordeDefense::EnemyStats::getStats(enemy.type).baseSpeed;
					hordeEnemies[enemy.id] = enemy;
					
					std::cout << "Enemy spawned: ID=" << enemy.id << " Type=" << (int)enemy.type << std::endl;
				}
				else if (p.header == headerHordeEnemyUpdate)
				{
					// Server sends ARRAY of enemy updates
					int numEnemies = event.packet->dataLength / sizeof(HordeEnemyUpdateData);
					HordeEnemyUpdateData* updates = (HordeEnemyUpdateData*)data;
					
					for (int i = 0; i < numEnemies; i++)
					{
						auto& updateData = updates[i];
						auto it = hordeEnemies.find(updateData.enemyId);
						if (it != hordeEnemies.end())
						{
							it->second.position = glm::vec2(updateData.posX, updateData.posY);
							it->second.health = updateData.health;
							it->second.targetPlayerId = updateData.targetPlayerId;
						}
					}
				}
				else if (p.header == headerHordeEnemyDeath)
				{
					auto deathData = *(HordeEnemyDeathData*)data;
					hordeEnemies.erase(deathData.enemyId);
					
					// Decrement enemy count for UI display
					if (enemiesAlive > 0) {
						enemiesAlive--;
					}
					
					// Log enemy kill (money update comes from server via headerHordePlayerMoneyUpdate)
					if (deathData.killerCid == cid)
					{
						std::cout << "Enemy killed! +$" << deathData.moneyReward << " (awaiting server update)" << std::endl;
					}
				}
				else if (p.header == headerHordeEnemyAttack)
				{
					auto attackData = *(HordeEnemyAttackData*)data;
							// Find the target player and apply damage
				auto playerIt = players.find(attackData.targetCid);
				if (playerIt != players.end())
				{
					phisics::Entity& targetPlayer = playerIt->second;
					
					int remainingDamage = attackData.damage;
					
					// Check if player has shield first
					if (targetPlayer.shieldHealth > 0)
					{
						// Shield absorbs damage
						int shieldDamage = std::min((int)targetPlayer.shieldHealth, remainingDamage);
						targetPlayer.shieldHealth -= shieldDamage;
						remainingDamage -= shieldDamage;
						
						if (attackData.targetCid == cid)
						{
							std::cout << "Shield absorbed " << shieldDamage << " damage! Shield remaining: " << targetPlayer.shieldHealth << std::endl;
						}
					}
					
					// Apply remaining damage to health
					if (remainingDamage > 0)
					{
						targetPlayer.life -= remainingDamage;
						if (targetPlayer.life < 0) targetPlayer.life = 0;
						
						if (attackData.targetCid == cid)
						{
							std::cout << "Enemy hit you for " << remainingDamage << " damage! Health: " << targetPlayer.life << "/" << targetPlayer.maxLife << std::endl;					}
				}
			}
				}
				else if (p.header == headerHordeDamageUpdate)
				{
					// Handle batched damage leaderboard updates (lightweight, high frequency)
					int updateCount = size / sizeof(HordeDamageUpdate);
					HordeDamageUpdate* updates = (HordeDamageUpdate*)data;
					
					for (int i = 0; i < updateCount; i++)
					{
						auto playerIt = players.find(updates[i].cid);
						if (playerIt != players.end())
						{
							// Only update damage stats, not position or other fields
							playerIt->second.totalDamageDealt = updates[i].totalDamageDealt;
							playerIt->second.enemiesKilled = updates[i].enemiesKilled;
						}
					}
				}
				else if (p.header == headerHordeEnemyAttack)
				{
					auto attackData = *(HordeEnemyAttackData*)data;
							// Find the target player and apply damage
				auto playerIt = players.find(attackData.targetCid);
				if (playerIt != players.end())
				{
					phisics::Entity& targetPlayer = playerIt->second;
					
					int remainingDamage = attackData.damage;
					
					// Check if player has shield first
					if (targetPlayer.shieldHealth > 0)
					{
						// Shield absorbs damage
						int shieldDamage = std::min((int)targetPlayer.shieldHealth, remainingDamage);
						targetPlayer.shieldHealth -= shieldDamage;
						remainingDamage -= shieldDamage;
						
						if (attackData.targetCid == cid)
						{
							std::cout << "Shield absorbed " << shieldDamage << " damage! Shield remaining: " << targetPlayer.shieldHealth << std::endl;
						}
					}
					
					// Apply remaining damage to health
					if (remainingDamage > 0)
					{
						targetPlayer.life -= remainingDamage;
						if (targetPlayer.life < 0) targetPlayer.life = 0;
						
						if (attackData.targetCid == cid)
						{
							std::cout << "Enemy hit you for " << remainingDamage << " damage! Health: " << targetPlayer.life << "/" << targetPlayer.maxLife << std::endl;					}
				}
			}
				}
				else if (p.header == headerHordeWaveStart)
				{
					auto waveData = *(HordeWaveStartData*)data;
					currentWave = waveData.waveNumber;
					enemiesAlive = waveData.totalEnemies; // Set initial enemy count for wave
					
					waveNotification = "Wave " + std::to_string(currentWave) + " Starting!";
					waveNotificationTimer = 3.0f;
					
					std::cout << "Wave " << currentWave << " started! Total enemies: " << enemiesAlive << std::endl;
				}
				else if (p.header == headerHordeWaveComplete)
				{
					auto completeData = *(HordeWaveCompleteData*)data;
					
					waveNotification = "Wave Complete! Bonus: $" + std::to_string(completeData.completionBonus);
				 waveNotificationTimer = 3.0f;
					
					// Note: Money update comes from server via headerHordePlayerMoneyUpdate
					// (no local money tracking to avoid double-counting)
					if (completeData.mvpPlayerId == cid)
					{
						waveNotification += " (MVP!)";
					}
					
					std::cout << "Wave " << completeData.waveNumber << " complete! MVP: " << completeData.mvpPlayerId << std::endl;
				}
				else if (p.header == headerHordePlayerMoneyUpdate)
				{
					auto moneyData = *(HordePlayerMoneyUpdate*)data;
					if (moneyData.cid == cid)
					{
						playerMoney = moneyData.newMoney;
						std::cout << "Money updated: $" << playerMoney << " (" << moneyData.reason << ")" << std::endl;
					}
				}
				else if (p.header == headerHordePlayerStatsUpdate)
				{
					auto statsData = *(HordePlayerStatsUpdate*)data;
					auto it = players.find(statsData.cid);
					if (it != players.end())
					{
						// Update player upgrade levels and buffs
						it->second.damageUpgradeLevel = statsData.damageLevel;
						it->second.fireRateUpgradeLevel = statsData.fireRateLevel;
						it->second.healthUpgradeLevel = statsData.healthLevel;
						it->second.speedUpgradeLevel = statsData.speedLevel;
						it->second.bulletSpeedUpgradeLevel = statsData.bulletSpeedLevel;
						it->second.speedBoostWaves = statsData.speedBoostWaves;
						it->second.damageBoostWaves = statsData.damageBoostWaves;
						it->second.multiShotWaves = statsData.multiShotWaves;
						it->second.shieldHealth = statsData.shieldHealth;
						
						// Update local upgrade levels for UI
						if (statsData.cid == cid)
						{
							playerUpgrades.damageLevel = statsData.damageLevel;
							playerUpgrades.fireRateLevel = statsData.fireRateLevel;
							playerUpgrades.healthLevel = statsData.healthLevel;
							playerUpgrades.speedLevel = statsData.speedLevel;
							playerUpgrades.bulletSpeedLevel = statsData.bulletSpeedLevel;
						}
					}
				}
				else if (p.header == headerHordeBuyUpgradeResponse)
				{
					auto response = *(HordeBuyUpgradeResponse*)data;
					
					if (response.success)
					{
						playerMoney = response.newMoney;
						auto upgradeType = static_cast<HordeDefense::UpgradeType>(response.upgradeType);
						auto upgradeInfo = HordeDefense::UpgradeInfo::getInfo(upgradeType);
						
						lastShopMessage = "Purchased: " + std::string(upgradeInfo.name) + " (Level " + std::to_string(response.newLevel) + ")";
						shopMessageTimer = 3.0f;
						
						std::cout << "Upgrade successful: " << upgradeInfo.name << " -> Level " << response.newLevel << std::endl;
					}
					else
					{
						lastShopMessage = "Purchase Failed: " + std::string(response.message);
						shopMessageTimer = 3.0f;
						std::cout << "Upgrade failed: " << response.message << std::endl;
					}
				}
				else if (p.header == headerHordeBuyItemResponse)
				{
					auto response = *(HordeBuyItemResponse*)data;
					
					if (response.success)
					{
						playerMoney = response.newMoney;
						auto itemType = static_cast<HordeDefense::ShopItemType>(response.itemType);
						auto itemInfo = HordeDefense::ShopItemInfo::getInfo(itemType);
						
						lastShopMessage = "Purchased: " + std::string(itemInfo.name);
						shopMessageTimer = 3.0f;
						
						std::cout << "Item purchased: " << itemInfo.name << std::endl;
					}
					else
					{
						lastShopMessage = "Purchase Failed: " + std::string(response.message);
						shopMessageTimer = 3.0f;
						std::cout << "Item purchase failed: " << response.message << std::endl;
					}
				}
			

			// ========================================================================
			// BOSS FIGHT PACKET HANDLERS
			// ========================================================================
			else if (p.header == headerBossFightStateUpdate)
				{
					auto stateData = *(BossFightStateUpdateData*)data;
					bossFightState = static_cast<BossFight::BossFightState>(stateData.gameState);
					clientBoss.health = stateData.bossHealth;
					clientBoss.maxHealth = stateData.bossMaxHealth;
					clientBoss.currentPhase = static_cast<BossFight::BossPhase>(stateData.bossPhase);
				}
				else if (p.header == headerBossFightBossSpawn)
				{
					auto spawnData = *(BossFightBossSpawnData*)data;
					clientBoss.bossId = spawnData.bossId;
					clientBoss.type = static_cast<BossFight::BossType>(spawnData.bossType);
					clientBoss.bossLevel = spawnData.bossLevel;  // Store boss level for texture selection
					clientBoss.position = glm::vec2(spawnData.posX, spawnData.posY);
					clientBoss.health = spawnData.health;
					clientBoss.maxHealth = spawnData.maxHealth;
					clientBoss.speed = spawnData.speed;
					clientBoss.isAlive = true;
					
					bossNotification = "BOSS SPAWNED!";
					bossNotificationTimer = 3.0f;
					
					std::cout << "[BossFight] Boss spawned! Level: " << clientBoss.bossLevel << std::endl;
				}
				else if (p.header == headerBossFightBossUpdate)
				{
					auto updateData = *(BossFightBossUpdateData*)data;
					clientBoss.position = glm::vec2(updateData.posX, updateData.posY);
					clientBoss.velocity = glm::vec2(updateData.velX, updateData.velY);
					clientBoss.health = updateData.health;
					clientBoss.currentPhase = static_cast<BossFight::BossPhase>(updateData.currentPhase);
					clientBoss.currentTargetId = updateData.targetPlayerId;
				}
				else if (p.header == headerBossFightBossAttack)
				{
					auto attackData = *(BossFightBossAttackData*)data;
					lastBossAttack = static_cast<BossFight::BossAttackType>(attackData.attackType);
					bossAttackAnimTimer = 1.0f;
					
					if (lastBossAttack == BossFight::BossAttackType::CIRCLE_SPRAY)
					{
						bossNotification = "CIRCLE SPRAY!";
						bossNotificationTimer = 2.0f;
					}
					else if (lastBossAttack == BossFight::BossAttackType::MELEE)
					{
						bossNotification = "BOSS ATTACKS!";
						bossNotificationTimer = 1.0f;
					}
					
					std::cout << "[BossFight] Boss attack: " << (int)lastBossAttack << std::endl;
				}
				else if (p.header == headerBossFightCircleSpray)
				{
					auto sprayData = *(BossFightCircleSprayData*)data;
					bossNotification = "CIRCLE SPRAY ATTACK!";
					bossNotificationTimer = 2.0f;
					
					std::cout << "[BossFight] Boss circle spray at (" << sprayData.centerX << ", " << sprayData.centerY << ")" << std::endl;

					// Spawn client-side bullets
					float angleStep = 2.0f * 3.14159f / sprayData.bulletCount;
					for (int i = 0; i < sprayData.bulletCount; i++) {
						float angle = i * angleStep;
						ClientBossBullet b;
						b.pos = glm::vec2(sprayData.centerX, sprayData.centerY);
						b.velocity = glm::vec2(std::cos(angle), std::sin(angle)) * sprayData.bulletSpeed;
						b.lifetime = 5.0f;
						clientBossBullets.push_back(b);
					}
				}
				else if (p.header == headerBossFightBossDeath)
				{
					auto deathData = *(BossFightBossDeathData*)data;
					clientBoss.isAlive = false;
					bossNotification = "BOSS DEFEATED!";
					bossNotificationTimer = 5.0f;
					
					std::cout << "[BossFight] Boss defeated by player " << deathData.lastHitPlayerCid << std::endl;
				}
				else if (p.header == headerBossFightPlayerRespawn)
				{
					auto respawnData = *(BossFightPlayerRespawnData*)data;
					auto it = players.find(respawnData.cid);
					if (it != players.end())
					{
						it->second.pos = glm::vec2(respawnData.posX, respawnData.posY);
						it->second.lastPos = it->second.pos;
						it->second.life = it->second.maxLife;
					}
					
					if (respawnData.cid == cid)
					{
						bossNotification = "RESPAWNED!";
						bossNotificationTimer = 2.0f;
					}
					
					std::cout << "[BossFight] Player " << respawnData.cid << " respawned" << std::endl;
				}
				else if (p.header == headerBossFightMatchEnd)
				{
					auto endData = *(BossFightMatchEndData*)data;
					matchEnded = true;
					
					if (endData.victory)
					{
						bossNotification = "VICTORY!";
					}
					else
					{
						bossNotification = "DEFEATED!";
					}
					bossNotificationTimer = 10.0f;
					
					std::cout << "[BossFight] Match ended - Victory: " << endData.victory << std::endl;
				}
				else if (p.header == headerBossFightPlayerDamage)
				{
					auto damageData = *(BossFightPlayerDamageData*)data;
					if (damageData.cid == cid)
					{
						auto& player = players[cid];
						player.life -= damageData.damage;
						if (player.life <= 0)
						{
							player.life = 0;
						}
						player.hitTime = phisics::Entity::invincibilityTime;
						
						std::cout << "[BossFight] You took " << damageData.damage << " damage! HP: " << player.life << std::endl;
					}
				}
				else if (p.header == headerHordeBossTeleport)
				{
					if (currentGameMode == GameMode::HORDE_DEFENSE)
					{
						struct BossTeleportData { int32_t enemyId; float x, y; };
						BossTeleportData* teleportData = (BossTeleportData*)data;
						
						// Find boss local enemy
						for (auto& [id, enemy] : hordeEnemies) {
							if (id == teleportData->enemyId) {
								// Teleport effect (poof)
								// Simply update pos for now, server handles sync
								enemy.position = {teleportData->x, teleportData->y};
								// std::cout << "Boss Teleported!" << std::endl;
								break;
							}
						}
					}
				}
				else if (p.header == headerHordeBossAttack)
				{
					if (currentGameMode == GameMode::HORDE_DEFENSE)
					{
						struct BossAttackData { 
							int32_t enemyId; 
							float startX, startY;
							float dirX, dirY;
						};
						BossAttackData* attackData = (BossAttackData*)data;
						
						// Spawn a visual projectile using hordeBossBullets (like Boss Fight mode)
						ClientBossBullet b;
						b.pos = {attackData->startX, attackData->startY};
						b.velocity = glm::vec2(attackData->dirX, attackData->dirY) * 8.0f; // Bullet speed
						b.lifetime = 10.0f; // Lifetime before auto-despawn
						
						hordeBossBullets.push_back(b);
					}
				}
				enet_packet_destroy(event.packet);

				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT:
			{
				// Server disconnected - reset client state and return to menu
				std::cout << "Disconnected from server" << std::endl;
				joined = false;
				server = nullptr;
				matchEnded = true;  // Signal match end for UI
				break;
			}
		}
	}

}
#include <AccountManager.h>
void closeFunction(AccountManager &accountManager)
{
	if (!server) { return; }
	
	ENetEvent event;

	Packet p;
	p.header = gameEndHeader;
	p.cid = cid;

				// If you want to send some data (e.g., final score)

	auto& player = players[cid];
	int32_t finalScore = players[cid].kills; // Example: using kills as final score
	Account* account = accountManager.getAccount(player.name);
	account->totalScore += finalScore;
	accountManager.updateAccount(*account);
	std::cout << "Account old info" << account->username << " " << account->email << " total score: " << account->totalScore - finalScore << "\n";
	sendPacket(server, p, (const char*)&finalScore, sizeof(int32_t), true, 1);

	enet_peer_disconnect(server, 0);
	//wait for disconect
	while (enet_host_service(client, &event, 10) > 0)
	{
		switch (event.type)
		{
			case ENET_EVENT_TYPE_RECEIVE:
			{
				enet_packet_destroy(event.packet);
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT:
			{
				break;
			}
		}
	}


}


void clientFunction(float deltaTime, gl2d::Renderer2D &renderer, Textures textures, std::string ip, char *playerName, int port)
{

	if (!joined)
	{
		if (!client)
		{
			client = enet_host_create(nullptr, 1, 1, 0, 0);
		}

		if (connectToServer(client, server, cid, ip, playerName, port))
		{
			joined = true;
		}
	}
	else
	{

		msgLoop(client);

		auto &player = players[cid];

	#pragma region input
		// Base stats (lowered to make upgrades more noticeable)
		float baseSpeed = 6 * deltaTime;
		float baseBulletSpeed = 16;
		float baseFireRateCooldown = 0.5;
		
		// Apply Horde Defense upgrades
		float speed = baseSpeed;
		float bulletSpeed = baseBulletSpeed;
		float fireRateCooldown = baseFireRateCooldown;
		
		if (currentGameMode == GameMode::HORDE_DEFENSE)
		{
			// Speed upgrade: +15% per level
			float speedMultiplier = 1.0f + (player.speedUpgradeLevel * 0.15f);
			if (player.speedBoostWaves > 0) speedMultiplier += 0.5f;  // +50% from speed boost item
			speed = baseSpeed * speedMultiplier;
			
			// Fire rate upgrade: +20% per level (reduces cooldown)
			float fireRateMultiplier = 1.0f + (player.fireRateUpgradeLevel * 0.20f);
			fireRateCooldown = baseFireRateCooldown / fireRateMultiplier;
			
			// Bullet speed upgrade: +30% per level
			float bulletSpeedMultiplier = 1.0f + (player.bulletSpeedUpgradeLevel * 0.30f);
			bulletSpeed = baseBulletSpeed * bulletSpeedMultiplier;
		}
		
		float posy = 0;
		float posx = 0;
		constexpr float CONTROLLER_MARGIN = 0.5;
		
		// Block player inputs when deathmatch match has ended (only ESC allowed)
		bool inputBlocked = (matchEnded && currentGameMode == GameMode::DEATHMATCH);

		if (!inputBlocked && (platform::isKeyHeld(platform::Button::Up)
			|| platform::isKeyHeld(platform::Button::W)
			|| platform::getControllerButtons().buttons[platform::ControllerButtons::Up].held
			|| platform::getControllerButtons().LStick.y < -CONTROLLER_MARGIN
			))
		{
			posy = -1;
		}
		if (!inputBlocked && (platform::isKeyHeld(platform::Button::Down)
			|| platform::isKeyHeld(platform::Button::S)
			|| platform::getControllerButtons().buttons[platform::ControllerButtons::Down].held
			|| platform::getControllerButtons().LStick.y > CONTROLLER_MARGIN
			))
		{
			posy = 1;
		}
		if (!inputBlocked && (platform::isKeyHeld(platform::Button::Left)
			|| platform::isKeyHeld(platform::Button::A)
			|| platform::getControllerButtons().buttons[platform::ControllerButtons::Left].held
			|| platform::getControllerButtons().LStick.x < -CONTROLLER_MARGIN
			))
		{
			posx = -1;
		}
		if (!inputBlocked && (platform::isKeyHeld(platform::Button::Right)
			|| platform::isKeyHeld(platform::Button::D)
			|| platform::getControllerButtons().buttons[platform::ControllerButtons::Right].held
			|| platform::getControllerButtons().LStick.x > CONTROLLER_MARGIN
			))
		{
			posx = 1;
		}

		// Boss Fight: Start match when Enter is pressed during waiting state
		if (currentGameMode == GameMode::BOSS_FIGHT && 
		    currentMatchState == MatchState::MATCH_WAITING && 
		    bossFightState == BossFight::BossFightState::WAITING)
		{
			if (platform::isKeyPressedOn(platform::Button::Enter))
			{
				// Send boss fight start request to server
				if (joined && server)
				{
					Packet p;
					p.header = headerBossFightStartRequest;
					p.cid = cid;
					sendPacket(server, p, nullptr, 0, true, 0);
					std::cout << "Sent boss fight start request to server (CID: " << cid << ")" << std::endl;
				}
			}
		}
		else if (platform::isKeyPressedOn(platform::Button::Enter))
		{
			// Fullscreen toggle (only when not in boss fight waiting state)
			platform::setFullScreen(!platform::isFullScreen());
		}
		
		// Toggle shop UI with 'B' key (only during buy phase in Horde Defense)
		if (platform::isKeyPressedOn(platform::Button::B))
		{
			if (currentGameMode == GameMode::HORDE_DEFENSE && 
			    hordeState == HordeDefense::HordeDefenseState::BUYING_PHASE)
			{
				showShopUI = !showShopUI;
				
				// Reset selections when opening shop
				if (showShopUI)
				{
					selectedShopTab = 0;
					selectedUpgradeIndex = 0;
					selectedItemIndex = 0;
					lastShopMessage = "";
					shopMessageTimer = 0.0f;
				}
			}
		}
		
		// Shop UI navigation (only when shop is open)
		if (showShopUI && currentGameMode == GameMode::HORDE_DEFENSE)
		{
			// Tab switching (1/2 keys or Left/Right shoulder buttons)
			if (platform::isKeyPressedOn(platform::Button::NR1) || 
			    platform::getControllerButtons().buttons[platform::ControllerButtons::LBumper].pressed)
			{
				selectedShopTab = 0;  // Upgrades tab
			}
			if (platform::isKeyPressedOn(platform::Button::NR2) || 
			    platform::getControllerButtons().buttons[platform::ControllerButtons::RBumper].pressed)
			{
				selectedShopTab = 1;  // Items tab
			}
			
			// Navigation within current tab
			if (selectedShopTab == 0)  // Upgrades
			{
				if (platform::isKeyPressedOn(platform::Button::Up) || platform::isKeyPressedOn(platform::Button::W))
				{
					selectedUpgradeIndex--;
					if (selectedUpgradeIndex < 0) selectedUpgradeIndex = HordeDefense::UPGRADE_COUNT - 1;
				}
				if (platform::isKeyPressedOn(platform::Button::Down) || platform::isKeyPressedOn(platform::Button::S))
				{
					selectedUpgradeIndex++;
					if (selectedUpgradeIndex >= HordeDefense::UPGRADE_COUNT) selectedUpgradeIndex = 0;
				}
				
				// Purchase upgrade (Space/E key or A button)
				if (platform::isKeyPressedOn(platform::Button::Space) || 
				    platform::isKeyPressedOn(platform::Button::E) ||
				    platform::getControllerButtons().buttons[platform::ControllerButtons::A].pressed)
				{
					// Send buy upgrade request
					HordeBuyUpgradeData buyData;
					buyData.upgradeType = selectedUpgradeIndex;
					buyData.currentLevel = playerUpgrades.getLevel(static_cast<HordeDefense::UpgradeType>(selectedUpgradeIndex));
					
					Packet p;
					p.cid = cid;
					p.header = headerHordeBuyUpgrade;
					sendPacket(server, p, (const char*)&buyData, sizeof(buyData), true, 1);
					
					std::cout << "Requesting upgrade purchase: Type " << selectedUpgradeIndex << std::endl;
				}
			}
			else  // Items
			{
				if (platform::isKeyPressedOn(platform::Button::Up) || platform::isKeyPressedOn(platform::Button::W))
				{
					selectedItemIndex--;
					if (selectedItemIndex < 0) selectedItemIndex = HordeDefense::SHOP_ITEM_COUNT - 1;
				}
				if (platform::isKeyPressedOn(platform::Button::Down) || platform::isKeyPressedOn(platform::Button::S))
				{
					selectedItemIndex++;
					if (selectedItemIndex >= HordeDefense::SHOP_ITEM_COUNT) selectedItemIndex = 0;
				}
				
				// Purchase item (Space/E key or A button)
				if (platform::isKeyPressedOn(platform::Button::Space) || 
				    platform::isKeyPressedOn(platform::Button::E) ||
				    platform::getControllerButtons().buttons[platform::ControllerButtons::A].pressed)
				{
					// Send buy item request
					HordeBuyItemData buyData;
					buyData.itemType = selectedItemIndex;
					
					Packet p;
					p.cid = cid;
					p.header = headerHordeBuyItem;
					sendPacket(server, p, (const char*)&buyData, sizeof(buyData), true, 1);
					
					std::cout << "Requesting item purchase: Type " << selectedItemIndex << std::endl;
				}
			}
		}

		static float culldown = 0;
		static int bateryShooting = 0;

		if (culldown > 0)
		{
			culldown -= deltaTime;
		}

		// Only allow shooting if player is alive and inputs not blocked
		if (!inputBlocked && (platform::isLMouseHeld() 
			||
			platform::getControllerButtons().LT > CONTROLLER_MARGIN
			)
			&& culldown <= 0.f
			&& player.life > 0)  // Check if player is alive
		{

			culldown = fireRateCooldown;  // Use calculated fire rate cooldown

			phisics::Bullet b;
			b.pos = player.pos + (player.dimensions/2.f);
			b.color = player.color;
			b.cid = cid;

			glm::vec2 thumbDir = {platform::getControllerButtons().RStick.x,platform::getControllerButtons().RStick.y};

			glm::vec2 baseDirection;
			if (glm::length(thumbDir) > 0.f)
			{
				thumbDir = glm::normalize(thumbDir);
				baseDirection = thumbDir;
			}
			else
			{
				// Convert mouse position from screen-space to world-space
				auto mousePos = platform::getRelMousePosition();
				
				// Convert mouse from screen space to world space using camera
				glm::vec2 mouseWorldPos = renderer.currentCamera.convertPoint(mousePos, renderer.windowW, renderer.windowH);
				
				// Get player center in world coordinates  
				glm::vec2 playerCenter = (player.pos + player.dimensions / 2.f) * worldMagnification;
				
				// Calculate direction from player to mouse in world space
				auto delta = mouseWorldPos - playerCenter;

				float magnitude = glm::length(delta);
				if (magnitude == 0)
				{
					baseDirection = {1,0};
				}
				else
				{
					baseDirection = delta / magnitude;
				}
			}

			// Check if multi-shot is active
			if (currentGameMode == GameMode::HORDE_DEFENSE && player.multiShotWaves > 0)
			{
				// Shoot 3 bullets in a spread pattern
				float spreadAngle = 0.3f; // ~17 degrees spread
				
				// Center bullet
				b.direction = baseDirection;
				Packet p;
				p.cid = cid;
				p.header = headerSendBullet;
				sendPacket(server, p, (const char *)&b, sizeof(phisics::Bullet), true, 1);
				ownBullets.push_back(b);
				
				// Play shooting sound
				AudioManager::getInstance().playShoot();
				
				// Left bullet (rotated counterclockwise)
				phisics::Bullet bLeft = b;
				float cosLeft = std::cos(-spreadAngle);
				float sinLeft = std::sin(-spreadAngle);
				bLeft.direction = glm::vec2(
					baseDirection.x * cosLeft - baseDirection.y * sinLeft,
					baseDirection.x * sinLeft + baseDirection.y * cosLeft
				);
				bLeft.direction = glm::normalize(bLeft.direction);
				sendPacket(server, p, (const char *)&bLeft, sizeof(phisics::Bullet), true, 1);
				ownBullets.push_back(bLeft);
				
				// Right bullet (rotated clockwise)
				phisics::Bullet bRight = b;
				float cosRight = std::cos(spreadAngle);
				float sinRight = std::sin(spreadAngle);
				bRight.direction = glm::vec2(
					baseDirection.x * cosRight - baseDirection.y * sinRight,
					baseDirection.x * sinRight + baseDirection.y * cosRight
				);
				bRight.direction = glm::normalize(bRight.direction);
				sendPacket(server, p, (const char *)&bRight, sizeof(phisics::Bullet), true, 1);
				ownBullets.push_back(bRight);
			}
			else
			{
				// Normal single bullet
				b.direction = baseDirection;
				Packet p;
				p.cid = cid;
				p.header = headerSendBullet;
				sendPacket(server, p, (const char *)&b, sizeof(phisics::Bullet), true, 1);
				ownBullets.push_back(b);
				
				// Play shooting sound
				AudioManager::getInstance().playShoot();
			}

			if (hasBatery)
			{
				bateryShooting = 25;
			}
			hasBatery = false;
		}

		if (bateryShooting > 0 && player.life > 0)  // Only shoot battery if alive
		{

			static float batteryShootingDellay = 0;
			constexpr float batteryShootingDellayCulldownTime = 0.06;
			batteryShootingDellay -= deltaTime;

			if (batteryShootingDellay < 0.f)
			{
				batteryShootingDellay += batteryShootingDellayCulldownTime;
				
				phisics::Bullet b;
				b.pos = player.pos + (player.dimensions / 2.f);
				b.color = player.color;
				b.cid = cid;
				b.direction = {1,0};
				
				float angle = (bateryShooting / 10.f) * 2.f * 3.14159265;

				b.direction = glm::mat2(std::cos(angle), -std::sin(angle), std::sin(angle), std::cos(angle)) * b.direction;
				b.direction = glm::normalize(b.direction);

				Packet p;
				p.cid = cid;
				p.header = headerSendBullet;
				sendPacket(server, p, (const char *)&b, sizeof(phisics::Bullet), true, 1);

				ownBullets.push_back(b);

				bateryShooting--;
			}

		}

	#pragma endregion

	#pragma region items

		for (int i = 0; i < items.size(); i++)
		{
			//pickup item
			if (items[i].checkCollisionPlayer(player))
			{
				Packet p;
				p.cid = cid;
				p.header = headerPickupItem;
				uint32_t itemId = items[i].itemId;
				sendPacket(server, p, (const char *)&itemId, sizeof(itemId), true, 1);
				//sendPacket(server, p, (const char*)&itemId, sizeof(itemId), true, 1);

				items.erase(items.begin() + i);
				i--;
				continue;
			}

		}

	#pragma endregion

	
	#pragma region player
		{

			bool playerChaged = 0;

			if (player.input.x != posx || player.input.y != posy)
			{
				playerChaged = true;
			}

			player.input = {posx, posy};

			for (auto &i : players)
			{
				glm::vec2 dir = i.second.input;
				if (dir.x != 0 || dir.y != 0)
				{
					i.second.move(glm::normalize(dir) * speed);
				}
				i.second.resolveConstrains(map);
				i.second.updateMove(deltaTime);
			}

			renderer.currentCamera.follow(player.pos * worldMagnification, deltaTime * 5, 3, renderer.windowW, renderer.windowH);
			renderer.currentCamera.clip(glm::vec2(map.w, map.h) *worldMagnification, {renderer.windowW, renderer.windowH});

			map.render(renderer, textures.sprites);

			for (auto &i : players)
			{
				i.second.draw(renderer, deltaTime, textures.character, textures.font);
			}
			
	#pragma region Horde Defense - Enemy Rendering
			if (currentGameMode == GameMode::HORDE_DEFENSE)
			{
				// Draw all enemies
				for (const auto& [enemyId, enemy] : hordeEnemies)
				{
					// Size multiplier
					// OLD BOSS was 5.0f. New Elites are 2.0f. New Bosses are 5.0f.
					float sizeMultiplier = 1.5f;  // Increased from 1.0f for better visibility
					if (enemy.type == HordeDefense::EnemyType::ELITE) sizeMultiplier = 2.5f;  // Increased from 2.0f
					else if (enemy.type >= HordeDefense::EnemyType::BOSS_WAVE5) sizeMultiplier = 5.5f;  // Slightly increased

					// Calculate screen position and size
					glm::vec4 enemyRect = {
						enemy.position.x * worldMagnification,
						enemy.position.y * worldMagnification,
						sizeMultiplier * worldMagnification,
						sizeMultiplier * worldMagnification
					};
					
					// Color based on enemy type
					glm::vec4 enemyColor;
					switch (enemy.type)
					{
						case HordeDefense::EnemyType::ZOMBIE:
							enemyColor = {0.3f, 0.8f, 0.3f, 1.0f};  // Green
							break;
						case HordeDefense::EnemyType::RUNNER:
							enemyColor = {1.0f, 0.6f, 0.2f, 1.0f};  // Orange
							break;
						case HordeDefense::EnemyType::TANK:
							enemyColor = {0.6f, 0.6f, 0.6f, 1.0f};  // Gray
							break;
						case HordeDefense::EnemyType::EXPLODER:
							enemyColor = {0.9f, 0.2f, 0.2f, 1.0f};  // Red
							break;
						case HordeDefense::EnemyType::ELITE:
							// Elite: Purple "Mini-Boss"
							enemyColor = {0.6f, 0.0f, 0.8f, 1.0f};
							break;
						case HordeDefense::EnemyType::BOSS_WAVE5: // Summoner (Gold)
							enemyColor = {1.0f, 0.8f, 0.0f, 1.0f}; 
							break;
						case HordeDefense::EnemyType::BOSS_WAVE10: // Bullet Hell (Red/Black Pulse)
						{
							float pulse = (std::sin(glfwGetTime() * 8.0f) + 1.0f) * 0.5f;
							enemyColor = glm::mix(glm::vec4(0.8f, 0.0f, 0.0f, 1.0f), glm::vec4(0.1f, 0.0f, 0.0f, 1.0f), pulse);
							break;
						}
						case HordeDefense::EnemyType::BOSS_WAVE15: // Explosive (Orange/Red)
							enemyColor = {1.0f, 0.4f, 0.0f, 1.0f};
							break;
						case HordeDefense::EnemyType::BOSS_WAVE20: // Final Boss (White/Cyan Pulse)
						{
							float pulse = (std::sin(glfwGetTime() * 10.0f) + 1.0f) * 0.5f;
							enemyColor = glm::mix(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), pulse);
							break;
						}
						default:
							enemyColor = {1.0f, 1.0f, 1.0f, 1.0f};
							break;
					}
				
				// Choose sprite texture based on enemy type
				gl2d::Texture* enemyTexture = nullptr;
				switch (enemy.type)
				{
					case HordeDefense::EnemyType::ZOMBIE:
						enemyTexture = &textures.zombieSprite;
						break;
					case HordeDefense::EnemyType::RUNNER:
						enemyTexture = &textures.runnerSprite;
						break;
					case HordeDefense::EnemyType::TANK:
						enemyTexture = &textures.tankSprite;
						break;
					case HordeDefense::EnemyType::EXPLODER:
						enemyTexture = &textures.exploderSprite;
						break;
					case HordeDefense::EnemyType::ELITE:
						enemyTexture = &textures.eliteSprite;
						break;
					case HordeDefense::EnemyType::BOSS_WAVE5:
						enemyTexture = &textures.bossSummonerSprite;
						break;
					case HordeDefense::EnemyType::BOSS_WAVE10:
						enemyTexture = &textures.bossBulletHellSprite;
						break;
					case HordeDefense::EnemyType::BOSS_WAVE15:
						enemyTexture = &textures.bossExploderSprite;
						break;
					case HordeDefense::EnemyType::BOSS_WAVE20:
						enemyTexture = &textures.bossFinalSprite;
						break;
					default:
						enemyTexture = &textures.zombieSprite; // Fallback
						break;
				}
				
				// Draw enemy with sprite (apply color tint for effects)
				if (enemyTexture && enemyTexture->id != 0) {
					renderer.renderRectangle(enemyRect, enemyColor, {}, 0.f, *enemyTexture);
				} else {
					// Fallback to colored rectangle if texture not loaded
					renderer.renderRectangle(enemyRect, enemyColor);
				}
					
					// Draw Boss Helper UI (For actual Bosses only, not Elites)
					bool isBoss = (enemy.type >= HordeDefense::EnemyType::BOSS_WAVE5);
					if (isBoss)
					{
						// "BOSS" Label (add type name)
						const char* bossName = "BOSS";
						if (enemy.type == HordeDefense::EnemyType::BOSS_WAVE5) bossName = "SUMMONER";
						else if (enemy.type == HordeDefense::EnemyType::BOSS_WAVE10) bossName = "BULLET HELL";
						else if (enemy.type == HordeDefense::EnemyType::BOSS_WAVE15) bossName = "EXPLODER";
						else if (enemy.type == HordeDefense::EnemyType::BOSS_WAVE20) bossName = "THE END";

						glm::vec2 labelPos = {enemyRect.x + (enemyRect.z / 2) - 20, enemyRect.y - 60};
						renderer.renderText(labelPos, bossName, textures.font, {1, 0, 0, 1}, 1.0f);
						
						// Boss Health Bar
						float healthPerc = enemy.health / enemy.maxHealth;
						glm::vec4 healthBarBg = {enemyRect.x, enemyRect.y - 25, enemyRect.z, 15};
						glm::vec4 healthBarFill = {enemyRect.x, enemyRect.y - 25, enemyRect.z * healthPerc, 15};
						
						renderer.renderRectangle(healthBarBg, {0.2f, 0.2f, 0.2f, 1.0f});
						renderer.renderRectangle(healthBarFill, {1.0f, 0.0f, 0.0f, 1.0f});
					}
					else 
					{
						// Normal Enemy Health Bar (Only if damaged)
						if (enemy.health < enemy.maxHealth) {
							float healthPerc = enemy.health / enemy.maxHealth;
							glm::vec4 healthBarBg = {enemyRect.x, enemyRect.y - 10, enemyRect.z, 5};
							glm::vec4 healthBarFill = {enemyRect.x, enemyRect.y - 10, enemyRect.z * healthPerc, 5};
							
							renderer.renderRectangle(healthBarBg, {0.2f, 0.2f, 0.2f, 1.0f});
							renderer.renderRectangle(healthBarFill, {0.0f, 1.0f, 0.0f, 1.0f});
						}
					}
				}
			}
			
			// ========================================================================
			// HORDE DEFENSE - Boss Bullet Rendering & Collision
			// ========================================================================
			if (currentGameMode == GameMode::HORDE_DEFENSE)
			{
				auto myPlayerIt = players.find(cid);
				glm::vec2 playerPos = myPlayerIt != players.end() ? myPlayerIt->second.pos : glm::vec2(0,0);
				
				for (auto it = hordeBossBullets.begin(); it != hordeBossBullets.end(); ) {
					// Update position
					it->pos += it->velocity * deltaTime;
					it->lifetime -= deltaTime;
					
					// Lifetime check
					if (it->lifetime <= 0) {
						it = hordeBossBullets.erase(it);
						continue;
					}
					
					// Collision with local player
					float playerLeft = playerPos.x;
					float playerRight = playerPos.x + 0.8f;
					float playerTop = playerPos.y;
					float playerBottom = playerPos.y + 0.8f;
					
					if (it->pos.x >= playerLeft && it->pos.x <= playerRight &&
						it->pos.y >= playerTop && it->pos.y <= playerBottom)
					{
						// Hit local player!
						it = hordeBossBullets.erase(it);
						
						// Send damage to server (1 damage per boss bullet)
						int damage = 1;
						Packet p;
						p.cid = cid;
						p.header = headerHordePlayerTakeDamage;
						sendPacket(server, p, (const char*)&damage, sizeof(damage), true, 1);
						
						std::cout << "Boss bullet hit you! Sending 1 damage to server." << std::endl;
						continue;
					}
					
					// Render bullet (purple color)
					glm::vec4 bulletRect = {
						it->pos.x * worldMagnification,
						it->pos.y * worldMagnification,
						0.5f * worldMagnification,
						0.5f * worldMagnification
					};
					renderer.renderRectangle(bulletRect, {0.8f, 0.0f, 1.0f, 1.0f}); // Purple bullets
					
					++it;
				}
			}
			
			// ========================================================================
			// BOSS FIGHT RENDERING
			// ========================================================================
			if (currentGameMode == GameMode::BOSS_FIGHT)
			{
				// Draw boss if alive
				if (clientBoss.isAlive)
				{
					// Calculate boss screen position (boss is 5x5 tiles)
					float bossSize = 5.0f;
					float bossSizeScreen = bossSize * worldMagnification;
					
					// Server uses center position, so we need to offset for top-left rendering
					glm::vec4 bossRect = {
						(clientBoss.position.x - bossSize/2.0f) * worldMagnification,
						(clientBoss.position.y - bossSize/2.0f) * worldMagnification,
						bossSizeScreen,
						bossSizeScreen
					};
					
					// Select texture based on boss level
					gl2d::Texture* bossTexture = nullptr;
					switch (clientBoss.bossLevel) {
						case 1:
							bossTexture = &textures.bossSummonerSprite;  // Level 1 (Easy)
							break;
						case 2:
							bossTexture = &textures.bossExploderSprite;  // Level 2 (Normal)
							break;
						case 3:
							bossTexture = &textures.bossFinalSprite;     // Level 3 (Hard)
							break;
						default:
							bossTexture = &textures.bossSummonerSprite;  // Fallback to level 1
							break;
					}
					
					// Draw boss sprite
				renderer.renderRectangle(bossRect, {1.0f, 1.0f, 1.0f, 1.0f}, {}, 0.f, *bossTexture);
				
				// DEBUG: Draw hitbox outline (red, 3px thick borders)
				// float outlineThickness = 3.0f;
				// glm::vec4 outlineColor = {1.0f, 0.0f, 0.0f, 0.8f};
				// // Top border
				// renderer.renderRectangle({bossRect.x, bossRect.y, bossRect.z, outlineThickness}, outlineColor);
				// // Bottom border
				// renderer.renderRectangle({bossRect.x, bossRect.y + bossRect.w - outlineThickness, bossRect.z, outlineThickness}, outlineColor);
				// // Left border
				// renderer.renderRectangle({bossRect.x, bossRect.y, outlineThickness, bossRect.w}, outlineColor);
				// // Right border
				// renderer.renderRectangle({bossRect.x + bossRect.z - outlineThickness, bossRect.y, outlineThickness, bossRect.w}, outlineColor);
				
				// DEBUG: Draw proximity damage zone (7 tile radius, tile-based visualization)
				constexpr float PROXIMITY_RADIUS = 5.0f;  // BOSS_CONTACT_RADIUS
				glm::vec4 proximityColor = {1.0f, 0.5f, 0.0f, 0.50f};  // Orange, 15% opacity for tiles
				
				// Get boss tile position (boss is centered)
				int bossTileX = static_cast<int>(clientBoss.position.x);
				int bossTileY = static_cast<int>(clientBoss.position.y);
				
				// Draw orange rectangle for each tile within proximity radius
				int radiusInTiles = static_cast<int>(PROXIMITY_RADIUS);
				for (int dx = -radiusInTiles; dx <= radiusInTiles; dx++) {
					for (int dy = -radiusInTiles; dy <= radiusInTiles; dy++) {
						// Calculate distance from boss center to this tile's center
						float tileCenterX = bossTileX + dx + 0.5f;
						float tileCenterY = bossTileY + dy + 0.5f;
						float distance = sqrt((tileCenterX - clientBoss.position.x) * (tileCenterX - clientBoss.position.x) + 
						                     (tileCenterY - clientBoss.position.y) * (tileCenterY - clientBoss.position.y));
						
						// Only draw if within radius
						if (distance <= PROXIMITY_RADIUS) {
							glm::vec4 tileRect = {
								(bossTileX + dx) * worldMagnification,
								(bossTileY + dy) * worldMagnification,
								worldMagnification,
								worldMagnification
							};
							renderer.renderRectangle(tileRect, proximityColor);
						}
					}
				}
				
					
					// Draw boss health bar (width of boss)
					float healthPercent = clientBoss.health / clientBoss.maxHealth;
					float healthBarWidth = bossSizeScreen;
					float healthBarHeight = 0.2f * worldMagnification;
					float healthBarY = (clientBoss.position.y - bossSize/2.0f - 0.5f) * worldMagnification;
					
					// Background (red)
					glm::vec4 bgRect = {
						(clientBoss.position.x - bossSize/2.0f) * worldMagnification,
						healthBarY,
						healthBarWidth,
						healthBarHeight
					};
					renderer.renderRectangle(bgRect, {0.5f, 0.0f, 0.0f, 0.9f});
					
					// Foreground (green to red gradient based on health)
					glm::vec4 healthColor = {1.0f - healthPercent, healthPercent, 0.0f, 1.0f};
					glm::vec4 fgRect = {
						(clientBoss.position.x - bossSize/2.0f) * worldMagnification,
						healthBarY,
						healthBarWidth * healthPercent,
						healthBarHeight
					};
					renderer.renderRectangle(fgRect, healthColor);
				}

				// Update and Render Boss Bullets
				for (auto it = clientBossBullets.begin(); it != clientBossBullets.end(); ) {
					// Update
					it->pos += it->velocity * deltaTime;
					it->lifetime -= deltaTime;

					if (it->lifetime <= 0) {
						it = clientBossBullets.erase(it);
						continue;
					}

					// Render
					glm::vec4 bulletRect = {
						it->pos.x * worldMagnification,
						it->pos.y * worldMagnification,
						0.5f * worldMagnification,
						0.5f * worldMagnification
					};
					// Orange/Red bullets
					renderer.renderRectangle(bulletRect, {1.0f, 0.5f, 0.0f, 1.0f});

					++it;
				}
			}
	#pragma endregion

			static float timer = 0;
			constexpr float updateTime = 1.f / 10;

			timer -= deltaTime;
			if (playerChaged || timer <= 0)
			{
				timer = updateTime;
				playerChaged = true;
			}

			if (playerChaged)
			{
				sendPlayerData(player, false);
			}
		}
	#pragma endregion

	#pragma region items

		for (int i = 0; i < items.size(); i++)
		{
			items[i].draw(renderer, textures.medKit, textures.battery);

		}

	#pragma endregion



	#pragma region bullets

		for (int i = 0; i < bullets.size(); i++)
		{
			bullets[i].updateMove(deltaTime * bulletSpeed);
			bullets[i].draw(renderer, textures.character);

			for (auto &e : players)
			{
				if (bullets[i].cid != e.first)
				{
					if (bullets[i].checkCollisionPlayer(e.second))
					{
						//hit player
						bullets.erase(bullets.begin() + i);
						i--;
						break;
					}
				}
			}
			
			// Horde Defense: Check enemy bullets (cid == -1) against LOCAL player
			if (currentGameMode == GameMode::HORDE_DEFENSE && bullets[i].cid == -1)
			{
				// Simple AABB collision with local player
				auto myPlayerIt = players.find(cid);
				if (myPlayerIt != players.end()) {
					glm::vec2 bulletPos = bullets[i].pos;
					float playerLeft = myPlayerIt->second.pos.x;
					float playerRight = myPlayerIt->second.pos.x + 0.8f;
					float playerTop = myPlayerIt->second.pos.y;
					float playerBottom = myPlayerIt->second.pos.y + 0.8f;
					
					if (bulletPos.x >= playerLeft && bulletPos.x <= playerRight &&
					    bulletPos.y >= playerTop && bulletPos.y <= playerBottom)
					{
						// Hit local player!
						bullets.erase(bullets.begin() + i);
						i--;
						
						// Send damage to server (1 damage per boss bullet)
						int damage = 1;
						Packet p;
						p.cid = cid;
						p.header = headerHordePlayerTakeDamage;
						sendPacket(server, p, (const char*)&damage, sizeof(damage), true, 1);
						
						std::cout << "Boss bullet hit you! Sending 1 damage to server." << std::endl;
						continue;
					}
				}
			}
			
			// Visual fix: Check collision with Boss
			if (currentGameMode == GameMode::BOSS_FIGHT && clientBoss.isAlive)
			{
				glm::vec2 bossCenter = clientBoss.position;
				float hitboxHalf = 2.5f; // Boss is 5x5
				glm::vec2 bulletPos = bullets[i].pos;

				if (bulletPos.x >= bossCenter.x - hitboxHalf && bulletPos.x <= bossCenter.x + hitboxHalf &&
					bulletPos.y >= bossCenter.y - hitboxHalf && bulletPos.y <= bossCenter.y + hitboxHalf) 
				{
					bullets.erase(bullets.begin() + i);
					i--;
					continue;
				}
			}
		}

		for (int i = 0; i < bullets.size(); i++)
		{
			if (bullets[i].checkCollisionMap(map))
			{
				bullets.erase(bullets.begin() + i);
				i--;
				continue;
			}
		}


		for (int i = 0; i < ownBullets.size(); i++)
		{
			ownBullets[i].updateMove(deltaTime * bulletSpeed);
			ownBullets[i].draw(renderer, textures.character);

			// Check collision with other players (Deathmatch mode)
			for (auto &e : players)
			{
				if (e.first != cid)
				{
					if (ownBullets[i].checkCollisionPlayer(e.second))
					{
						//hit player, register hit
						e.second.hit();

						Packet p;
						p.header = headerRegisterHit;
						p.cid = cid;
						sendPacket(server, p, (const char*)&e.first, sizeof(int32_t), true, 1);

						ownBullets.erase(ownBullets.begin() + i);
						i--;
						break;
					}
				}
			}
			
			// Visual fix: Check collision with Boss (damage handled by server)
			if (currentGameMode == GameMode::BOSS_FIGHT && clientBoss.isAlive)
			{
				glm::vec2 bossCenter = clientBoss.position;
				float hitboxHalf = 2.5f; // Boss is 5x5
				glm::vec2 bulletPos = ownBullets[i].pos;

				if (bulletPos.x >= bossCenter.x - hitboxHalf && bulletPos.x <= bossCenter.x + hitboxHalf &&
					bulletPos.y >= bossCenter.y - hitboxHalf && bulletPos.y <= bossCenter.y + hitboxHalf) 
				{
					ownBullets.erase(ownBullets.begin() + i);
					i--;
					continue;
				}
			}
			
			// Check collision with enemies (Horde Defense mode)
			if (currentGameMode == GameMode::HORDE_DEFENSE && i < ownBullets.size())
			{
				bool bulletHit = false;
				
				for (auto& [enemyId, enemy] : hordeEnemies)
				{
					// AABB Collision (Axis-Aligned Bounding Box) for precise hit detection
					// This fixes issues with the large square Boss hitbox
					float sizeMultiplier = 1.0f;
					if (enemy.type == HordeDefense::EnemyType::ELITE) sizeMultiplier = 2.0f;
					else if (enemy.type >= HordeDefense::EnemyType::BOSS_WAVE5) sizeMultiplier = 5.0f;
					
					// Enemy bounding box
					float enemyLeft = enemy.position.x;
					float enemyRight = enemy.position.x + sizeMultiplier;
					float enemyTop = enemy.position.y;
					float enemyBottom = enemy.position.y + sizeMultiplier;
					
					// Bullet bounding box (assuming ~0.5 size for bullet point)
					float bulletX = ownBullets[i].pos.x;
					float bulletY = ownBullets[i].pos.y;
					
					// Check if bullet is inside enemy box
					if (bulletX >= enemyLeft && bulletX <= enemyRight &&
						bulletY >= enemyTop && bulletY <= enemyBottom)
					{
						// Bullet hit enemy!
						// Send notification to server
						HordeBulletHitEnemyData hitData;
						hitData.enemyId = enemyId;
						hitData.damage = 10;  // Base damage, server will apply upgrades
						
						Packet p;
						p.header = headerHordeBulletHitEnemy;
						p.cid = cid;
						sendPacket(server, p, (const char*)&hitData, sizeof(hitData), true, 1);
						
						// Remove bullet
						ownBullets.erase(ownBullets.begin() + i);
						i--;
						bulletHit = true;
						
						// Optional: Add visual hit effect here
						// std::cout << "Hit enemy " << enemyId << "!" << std::endl;
						
						break;  // Bullet can only hit one enemy
					}
				}
				
				if (bulletHit)
				{
					continue;  // Skip to next bullet
				}
			}
		}

		for (int i = 0; i < ownBullets.size(); i++)
		{
			if (ownBullets[i].checkCollisionMap(map))
			{
				ownBullets.erase(ownBullets.begin() + i);
				i--;
				continue;
			}
		}

	#pragma endregion


	#pragma region ui
		{
			Ui::Frame f({0,0, renderer.windowW, renderer.windowH});

			auto c = renderer.currentCamera; //todo push pop camera
			renderer.currentCamera.setDefault();
			
	#pragma region Horde Defense HUD
			if (currentGameMode == GameMode::HORDE_DEFENSE)
			{
				// Top bar - Wave, State, and Money
				{
					// Wave display (top center) - increased padding from 20 to 50
					char waveText[64];
					snprintf(waveText, sizeof(waveText), "Wave: %d/%d", currentWave, totalWaves);
					glm::vec2 wavePos = glm::vec2(0.42f * renderer.windowW, 50.0f);
					renderer.renderText(wavePos, waveText, textures.font, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.8f, 4.f, 3.f, false);
					
					// State/Timer display (top center-left)
					if (hordeState == HordeDefense::HordeDefenseState::BUYING_PHASE)
					{
						char timerText[64];
						snprintf(timerText, sizeof(timerText), "Buy Phase: %ds", (int)phaseTimer);
						glm::vec2 timerPos = glm::vec2(0.35f * renderer.windowW, 90.0f);
						renderer.renderText(timerPos, timerText, textures.font, glm::vec4(0.2f, 1.0f, 0.8f, 1.0f), 0.7f, 4.f, 3.f, false);
						
						// Show shop hint
						glm::vec2 hintPos = glm::vec2(0.38f * renderer.windowW, 130.0f);
						renderer.renderText(hintPos, "Press B to open shop", textures.font, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), 0.5f, 4.f, 3.f, false);
					}
					else if (hordeState == HordeDefense::HordeDefenseState::WAVE_ACTIVE)
					{
						char enemyText[64];
						snprintf(enemyText, sizeof(enemyText), "Enemies: %d", enemiesAlive);
						glm::vec2 enemyPos = glm::vec2(0.38f * renderer.windowW, 90.0f);
						renderer.renderText(enemyPos, enemyText, textures.font, glm::vec4(1.0f, 0.4f, 0.4f, 1.0f), 0.7f, 4.f, 3.f, false);
					}
					
					// Money display (top right) - moved down to avoid overlap with health icons
					char moneyText[64];
					snprintf(moneyText, sizeof(moneyText), "Money: $%d", playerMoney);
					glm::vec2 moneyPos = glm::vec2(renderer.windowW - 350.0f, 120.0f);
					renderer.renderText(moneyPos, moneyText, textures.font, glm::vec4(1.0f, 0.9f, 0.2f, 1.0f), 0.8f, 4.f, 3.f, false);
				}
				
				// Active buffs display (below money)
				{
					auto it = players.find(cid);
					if (it != players.end())
					{
						const auto& player = it->second;
						float buffY = 160.0f;  // Updated to be below money (80 + 40 spacing)
						float buffX = renderer.windowW - 250.0f;
						
						if (player.speedBoostWaves > 0)
						{
							char buffText[32];
							snprintf(buffText, sizeof(buffText), "[Speed: %d wave%s]", player.speedBoostWaves, player.speedBoostWaves > 1 ? "s" : "");
							renderer.renderText(glm::vec2(buffX, buffY), buffText, textures.font, glm::vec4(0.2f, 0.8f, 1.0f, 1.0f), 0.5f, 4.f, 3.f, false);
							buffY += 25.0f;
						}
						if (player.damageBoostWaves > 0)
						{
							char buffText[32];
							snprintf(buffText, sizeof(buffText), "[Damage: %d]", player.damageBoostWaves);
							renderer.renderText(glm::vec2(buffX, buffY), buffText, textures.font, glm::vec4(1.0f, 0.5f, 0.2f, 1.0f), 0.5f, 4.f, 3.f, false);
							buffY += 25.0f;
						}
						if (player.multiShotWaves > 0)
						{
							char buffText[32];
							snprintf(buffText, sizeof(buffText), "[Multi-Shot: %d]", player.multiShotWaves);
							renderer.renderText(glm::vec2(buffX, buffY), buffText, textures.font, glm::vec4(0.9f, 0.2f, 0.9f, 1.0f), 0.5f, 4.f, 3.f, false);
							buffY += 25.0f;
						}
						if (player.shieldHealth > 0.0f)
						{
							char buffText[32];
							snprintf(buffText, sizeof(buffText), "[Shield: %.0f HP]", player.shieldHealth);
							renderer.renderText(glm::vec2(buffX, buffY), buffText, textures.font, glm::vec4(0.4f, 0.8f, 1.0f, 1.0f), 0.5f, 4.f, 3.f, false);
							buffY += 25.0f;
						}
					}
				}
				

				
				// Wave notifications (center screen)
				if (waveNotificationTimer > 0.0f)
				{
					float alpha = std::min(1.0f, waveNotificationTimer);
					glm::vec4 notifColor = glm::vec4(1.0f, 1.0f, 0.2f, alpha);
					glm::vec2 notifPos = glm::vec2(0.30f * renderer.windowW, 0.45f * renderer.windowH);
					renderer.renderText(notifPos, waveNotification.c_str(), textures.font, notifColor, 1.0f, 4.f, 3.f, false);
					waveNotificationTimer -= deltaTime;
				}
				
				// Victory/Defeat screens
				if (hordeState == HordeDefense::HordeDefenseState::VICTORY || hordeState == HordeDefense::HordeDefenseState::DEFEAT)
				{
					// Semi-transparent overlay
					auto overlayPos = Ui::Box().xLeftPerc(0.0).yTopPerc(0.0).xDimensionPercentage(1.0).yDimensionPercentage(1.0);
					renderer.renderRectangle(overlayPos, {0.0f, 0.0f, 0.0f, 0.7f});
					
					// Victory/Defeat text
					const char* resultText = (hordeState == HordeDefense::HordeDefenseState::VICTORY) ? "VICTORY!" : "DEFEAT!";
					glm::vec4 resultColor = (hordeState == HordeDefense::HordeDefenseState::VICTORY) ? 
						glm::vec4(0.2f, 1.0f, 0.2f, 1.0f) : glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);
					glm::vec2 resultPos = glm::vec2(0.38f * renderer.windowW, 0.35f * renderer.windowH);
					renderer.renderText(resultPos, resultText, textures.font, resultColor, 1.2f, 4.f, 3.f, false);
					
					// Details
					glm::vec2 detailPos = glm::vec2(0.35f * renderer.windowW, 0.45f * renderer.windowH);
					renderer.renderText(detailPos, waveNotification.c_str(), textures.font, Colors_White, 0.7f, 4.f, 3.f, false);
					
					// Instructions
					glm::vec2 instructPos = glm::vec2(0.35f * renderer.windowW, 0.55f * renderer.windowH);
					renderer.renderText(instructPos, "Press ESC to leave", textures.font, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), 0.5f, 4.f, 3.f, false);
				}
				
				// Shop UI (only during buy phase when toggled)
				if (showShopUI && hordeState == HordeDefense::HordeDefenseState::BUYING_PHASE)
				{
					// Dark semi-transparent overlay
					auto overlayPos = Ui::Box().xLeftPerc(0.0).yTopPerc(0.0).xDimensionPercentage(1.0).yDimensionPercentage(1.0);
					renderer.renderRectangle(overlayPos, {0.0f, 0.0f, 0.0f, 0.8f});
					
					// Shop window background
					float shopX = 0.15f * renderer.windowW;
					float shopY = 0.1f * renderer.windowH;
					float shopW = 0.7f * renderer.windowW;
					float shopH = 0.8f * renderer.windowH;
					auto shopBox = Ui::Box().xLeft(shopX).yTop(shopY).xDimensionPixels(shopW).yDimensionPixels(shopH);
					renderer.renderRectangle(shopBox, {0.1f, 0.1f, 0.15f, 0.95f});
					
					// Shop title
					glm::vec2 titlePos = glm::vec2(shopX + shopW * 0.38f, shopY + 20.0f);
					renderer.renderText(titlePos, "SHOP", textures.font, glm::vec4(1.0f, 0.9f, 0.2f, 1.0f), 1.2f, 4.f, 3.f, false);
					
					// Money display
					char moneyText[64];
					snprintf(moneyText, sizeof(moneyText), "Money: $%d", playerMoney);
					glm::vec2 moneyPos = glm::vec2(shopX + shopW * 0.35f, shopY + 70.0f);
					renderer.renderText(moneyPos, moneyText, textures.font, glm::vec4(0.2f, 1.0f, 0.2f, 1.0f), 0.9f, 4.f, 3.f, false);
					
					// Tab headers
					float tab1X = shopX + 50.0f;
					float tab2X = shopX + shopW * 0.5f + 20.0f;
					float tabY = shopY + 120.0f;
					
					glm::vec4 tab1Color = (selectedShopTab == 0) ? glm::vec4(1.0f, 1.0f, 0.2f, 1.0f) : glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
					glm::vec4 tab2Color = (selectedShopTab == 1) ? glm::vec4(1.0f, 1.0f, 0.2f, 1.0f) : glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
					
					renderer.renderText(glm::vec2(tab1X, tabY), "[1] UPGRADES", textures.font, tab1Color, 0.8f, 4.f, 3.f, false);
					renderer.renderText(glm::vec2(tab2X, tabY), "[2] ITEMS", textures.font, tab2Color, 0.8f, 4.f, 3.f, false);
					
					// Content area
					float contentY = tabY + 50.0f;
							if (selectedShopTab == 0)  // Upgrades tab
				{
					// List all upgrade types
					for (int i = 0; i < HordeDefense::UPGRADE_COUNT; i++)
					{
						auto upgradeType = static_cast<HordeDefense::UpgradeType>(i);
							auto upgradeInfo = HordeDefense::UpgradeInfo::getInfo(upgradeType);
							int currentLevel = playerUpgrades.getLevel(upgradeType);
							int cost = upgradeInfo.getCostForLevel(currentLevel + 1);
							
							float itemY = contentY + i * 80.0f;
							float itemX = shopX + 50.0f;
							
							// Selection highlight
							if (i == selectedUpgradeIndex)
							{
								auto highlightBox = Ui::Box().xLeft(itemX - 10.0f).yTop(itemY - 5.0f).xDimensionPixels(shopW - 80.0f).yDimensionPixels(75.0f);
								renderer.renderRectangle(highlightBox, {0.3f, 0.3f, 0.5f, 0.5f});
							}
							
							// Upgrade name and level
							char nameText[128];
							snprintf(nameText, sizeof(nameText), "%s [Level %d/%d]", upgradeInfo.name, currentLevel, upgradeInfo.maxLevel);
							glm::vec4 nameColor = (currentLevel >= upgradeInfo.maxLevel) ? 
								glm::vec4(0.5f, 0.5f, 0.5f, 1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
							renderer.renderText(glm::vec2(itemX, itemY), nameText, textures.font, nameColor, 0.7f, 4.f, 3.f, false);
							
							// Description
							renderer.renderText(glm::vec2(itemX, itemY + 25.0f), upgradeInfo.description, textures.font, 
								glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), 0.5f, 4.f, 3.f, false);
							
							// Cost
							if (currentLevel < upgradeInfo.maxLevel)
							{
								char costText[64];
								snprintf(costText, sizeof(costText), "Cost: $%d", cost);
								glm::vec4 costColor = (playerMoney >= cost) ? 
									glm::vec4(0.2f, 1.0f, 0.2f, 1.0f) : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
								renderer.renderText(glm::vec2(itemX, itemY + 50.0f), costText, textures.font, costColor, 0.6f, 4.f, 3.f, false);
							}
							else
							{
								renderer.renderText(glm::vec2(itemX, itemY + 50.0f), "MAX LEVEL", textures.font, 
									glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 0.6f, 4.f, 3.f, false);
							}
						}
					}				else  // Items tab
				{
					// List all shop items
					for (int i = 0; i < HordeDefense::SHOP_ITEM_COUNT; i++)
					{
						auto itemType = static_cast<HordeDefense::ShopItemType>(i);
							auto itemInfo = HordeDefense::ShopItemInfo::getInfo(itemType);
							
							float itemY = contentY + i * 80.0f;
							float itemX = shopX + 50.0f;
							
							// Selection highlight
							if (i == selectedItemIndex)
							{
								auto highlightBox = Ui::Box().xLeft(itemX - 10.0f).yTop(itemY - 5.0f).xDimensionPixels(shopW - 80.0f).yDimensionPixels(75.0f);
								renderer.renderRectangle(highlightBox, {0.3f, 0.3f, 0.5f, 0.5f});
							}
							
							// Item name
							renderer.renderText(glm::vec2(itemX, itemY), itemInfo.name, textures.font, 
								glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.7f, 4.f, 3.f, false);
							
							// Description
							renderer.renderText(glm::vec2(itemX, itemY + 25.0f), itemInfo.description, textures.font, 
								glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), 0.5f, 4.f, 3.f, false);
							
							// Cost
							char costText[64];
							snprintf(costText, sizeof(costText), "Cost: $%d", itemInfo.cost);
							glm::vec4 costColor = (playerMoney >= itemInfo.cost) ? 
								glm::vec4(0.2f, 1.0f, 0.2f, 1.0f) : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
							renderer.renderText(glm::vec2(itemX, itemY + 50.0f), costText, textures.font, costColor, 0.6f, 4.f, 3.f, false);
						}
					}
					
					// Instructions at bottom
					float instructY = shopY + shopH - 80.0f;
					renderer.renderText(glm::vec2(shopX + 50.0f, instructY), "W/S: Navigate  |  Space/E: Purchase  |  B: Close", 
						textures.font, glm::vec4(0.7f, 0.7f, 0.7f, 1.0f), 0.5f, 4.f, 3.f, false);
					
					// Shop message (purchase feedback)
					if (shopMessageTimer > 0.0f)
					{
						float alpha = std::min(1.0f, shopMessageTimer);
						glm::vec4 msgColor = glm::vec4(1.0f, 1.0f, 0.2f, alpha);
						renderer.renderText(glm::vec2(shopX + 50.0f, instructY + 40.0f), lastShopMessage.c_str(), 
							textures.font, msgColor, 0.6f, 4.f, 3.f, false);
						shopMessageTimer -= deltaTime;
					}
				}
			}
	#pragma endregion
		float xLeft = 0.95;
		float xSize = 0.05;  // Increased from 0.04 for better visibility
		float xAdvance = xSize - 0.03;  // Adjusted spacing

		// Debug: Log health values occasionally
		static float debugTimer = 0.0f;
		debugTimer += deltaTime;
		if (debugTimer > 2.0f && currentGameMode == GameMode::HORDE_DEFENSE)
		{
			std::cout << "[HealthRender] Rendering hearts: HP=" << player.life << "/" << player.maxLife << std::endl;
			debugTimer = 0.0f;
		}

		// Render health hearts: filled for current HP, empty for lost HP
		for (int i = 0; i < player.maxLife; i++)
		{
			auto crossPos = Ui::Box().xLeftPerc(xLeft).yTopPerc(0.02).xDimensionPercentage(xSize).yAspectRatio(1.f);
			auto crossPosDown = Ui::Box().xLeftPerc(xLeft+0.003).yTopPerc(0.025).xDimensionPercentage(xSize).yAspectRatio(1.f);
			
			if (i < player.life)
			{
				// Filled heart (current HP)
				renderer.renderRectangle(crossPosDown, {0.f,0.f,0.f,1.f}, {}, 0.f, textures.cross);
				renderer.renderRectangle(crossPos, {1.f,1.f,1.f,1.f}, {}, 0.f, textures.cross);
			}
			else
			{
				// Empty heart (lost HP) - render with transparency
				renderer.renderRectangle(crossPosDown, {0.f,0.f,0.f,0.5f}, {}, 0.f, textures.cross);
				renderer.renderRectangle(crossPos, {0.4f,0.4f,0.4f,0.5f}, {}, 0.f, textures.cross);
			}
			
			xLeft -= xAdvance;
		}
		xLeft = 0.95;

			if (hasBatery)
			{
				auto pos = Ui::Box().xLeftPerc(xLeft - 0.05).yTopPerc(0.035 + xSize).xDimensionPercentage(xSize).yAspectRatio(1.f);
				auto posDown = Ui::Box().xLeftPerc(xLeft + 0.003 - 0.05).yTopPerc(0.040 + xSize).xDimensionPercentage(xSize).yAspectRatio(1.f);
				renderer.renderRectangle(posDown, {0.f,0.f,0.f,1.f}, {}, 0.f, textures.battery);
				renderer.renderRectangle(pos, {1.f,1.f,1.f,1.f}, {}, 0.f, textures.battery);
			}
			
			// LEADERBOARD (Universal for all modes)
			{
				struct DisplayEntry {
					std::string name;
					int value;
					int extraValue;
					int32_t cid;
				};
				std::vector<DisplayEntry> displayList;
				std::string metricLabel = "Score";
				
				// 1. Determine Source of Data
				if (activeLeaderboard.count > 0)
				{
					// Use Server Packet Data
					for (int i=0; i < activeLeaderboard.count; ++i)
					{
						const auto& e = activeLeaderboard.entries[i];
						displayList.push_back({e.playerName, e.value, e.extraValue, e.cid});
					}
					
					if (activeLeaderboard.gameMode == (int)GameMode::DEATHMATCH) metricLabel = "Kills";
					else if (activeLeaderboard.gameMode == (int)GameMode::HORDE_DEFENSE) metricLabel = "Wave"; // Changed from Damage
					else if (activeLeaderboard.gameMode == (int)GameMode::BOSS_FIGHT) metricLabel = "Score";
				}
				else
				{
					// Fallback: Local Sorting (Old Method)
					// This ensures the leaderboard is visible immediately even before the first packet arrives
					std::vector<std::pair<int32_t, phisics::Entity*>> sortedPlayers;
					for (auto& p : players)
					{
						sortedPlayers.push_back({p.first, &p.second});
					}
					
					auto sortFunc = [currentGameMode](const auto& a, const auto& b) {
						if (currentGameMode == GameMode::DEATHMATCH) {
							return a.second->kills > b.second->kills;
						} else if (currentGameMode == GameMode::HORDE_DEFENSE) {
							// For HD, sort by Waves Survived
							if (a.second->wavesSurvived != b.second->wavesSurvived)
								return a.second->wavesSurvived > b.second->wavesSurvived;
							return a.second->totalDamageDealt > b.second->totalDamageDealt;
						}
						// For Boss Fight
						return a.second->totalDamageDealt > b.second->totalDamageDealt;
					};

					if (currentGameMode == GameMode::DEATHMATCH) {
						metricLabel = "Kills";
					} else if (currentGameMode == GameMode::HORDE_DEFENSE) {
						metricLabel = "Wave"; 
					} else if (currentGameMode == GameMode::BOSS_FIGHT) {
						metricLabel = "Score"; 
					}
					
					std::sort(sortedPlayers.begin(), sortedPlayers.end(), sortFunc);
					
					int count = std::min((int)sortedPlayers.size(), 5);
					for (int i=0; i<count; ++i)
					{
						const auto& p = *sortedPlayers[i].second;
						int val = 0;
						int extraVal = 0;
						if (currentGameMode == GameMode::DEATHMATCH) {
							val = p.kills;
							extraVal = p.deaths;
						}
						else if (currentGameMode == GameMode::HORDE_DEFENSE) {
							val = p.wavesSurvived;
							extraVal = p.totalDamageDealt;
						}
						else {
							val = p.totalDamageDealt;
						}
						
						displayList.push_back({p.name, val, extraVal, sortedPlayers[i].first});
					}
				}
				
				// 2. Render
				if (!displayList.empty())
				{
					float leaderboardX = 20.0f;
				float leaderboardY = 50.0f;
				
				// Dynamic width based on columns (HD has 4 columns, others have 3)
				float leaderboardW = (activeLeaderboard.gameMode == (int)GameMode::HORDE_DEFENSE) ? 540.0f : 460.0f;
				
				// Dynamic height based on number of entries
				float leaderboardH = 50.0f + (displayList.size() * 32.0f);
					
					auto leaderboardBox = Ui::Box()
						.xLeft(leaderboardX)
						.yTop(leaderboardY)
						.xDimensionPixels(leaderboardW)
						.yDimensionPixels(leaderboardH);
					renderer.renderRectangle(leaderboardBox, {0.0f, 0.0f, 0.0f, 0.7f});
					
					// Header
					glm::vec2 headerPos(leaderboardX + 10.0f, leaderboardY + 10.0f);
					renderer.renderText(headerPos, "LEADERBOARD", textures.font, 
						glm::vec4(1.0f, 0.8f, 0.2f, 1.0f), 0.65f, 4.f, 3.f, false);
					
					// Column headers
					glm::vec2 colHeaderPos(leaderboardX + 10.0f, leaderboardY + 38.0f);
					char colHeader[64];
					if (activeLeaderboard.gameMode == (int)GameMode::HORDE_DEFENSE) {
						// Special header for HD with two columns
						snprintf(colHeader, sizeof(colHeader), "Rank  Player   Wave   Damage");
					} else {
						snprintf(colHeader, sizeof(colHeader), "Rank  Player   %s", metricLabel.c_str());
					}
					renderer.renderText(colHeaderPos, colHeader, textures.font, 
						glm::vec4(0.6f, 0.6f, 0.6f, 1.0f), 0.45f, 4.f, 3.f, false);
					
					// Rows
					float rankY = leaderboardY + 62.0f;
					for (int i=0; i < (int)displayList.size(); ++i)
					{
						int rank = i + 1;
						const auto& entry = displayList[i];
						bool isLocalPlayer = (entry.cid == cid);
						
						// Colors
						glm::vec4 rankColor;
						if (rank == 1) rankColor = glm::vec4(1.0f, 0.85f, 0.0f, 1.0f);
						else if (rank == 2) rankColor = glm::vec4(0.85f, 0.85f, 0.85f, 1.0f);
						else if (rank == 3) rankColor = glm::vec4(0.8f, 0.5f, 0.3f, 1.0f);
						else rankColor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
						
						// Highlight local
						if (isLocalPlayer)
						{
							auto highlightBox = Ui::Box()
								.xLeft(leaderboardX + 5.0f)
								.yTop(rankY - 20.0f)
								.xDimensionPixels(leaderboardW - 10.0f)
								.yDimensionPixels(28.0f);
							renderer.renderRectangle(highlightBox, {0.2f, 0.4f, 0.6f, 0.4f});
							rankColor = glm::vec4(0.3f, 1.0f, 0.5f, 1.0f);
						}

						// Rank
						char rankText[8];
						snprintf(rankText, sizeof(rankText), "%d.", rank);
						renderer.renderText(glm::vec2(leaderboardX + 15.0f, rankY), rankText, 
							textures.font, rankColor, 0.52f, 4.f, 3.f, false);
						
						// Name
						char nameText[20];
						strncpy(nameText, entry.name.c_str(), 14);
						nameText[14] = '\0';
						renderer.renderText(glm::vec2(leaderboardX + 130.0f, rankY), nameText, 
							textures.font, rankColor, 0.4f, 4.f, 3.f, false);
						
						// Value 1 (Primary)
						char valText[32];
						snprintf(valText, sizeof(valText), "%d", entry.value);
						
						renderer.renderText(glm::vec2(leaderboardX + 300.0f, rankY), valText, 
							textures.font, rankColor, 0.52f, 4.f, 3.f, false);
							
						// Value 2 (Secondary - e.g. Damage in HD)
						if (activeLeaderboard.gameMode == (int)GameMode::HORDE_DEFENSE) {
							char extraValText[32];
							snprintf(extraValText, sizeof(extraValText), "%d", entry.extraValue);
							renderer.renderText(glm::vec2(leaderboardX + 420.0f, rankY), extraValText, 
								textures.font, rankColor, 0.52f, 4.f, 3.f, false);
						}
							
						rankY += 32.0f;
					}
				}
			}
			
			// Display kill message feed (top center)
			if (killMessageTimer > 0.0f)
			{
				glm::vec2 killMsgPos = glm::vec2(0.35f * renderer.windowW, 0.15f * renderer.windowH);
				float alpha = std::min(1.0f, killMessageTimer);
				glm::vec4 msgColor = glm::vec4(1.0f, 0.8f, 0.2f, alpha);
				renderer.renderText(killMsgPos, lastKillMessage.c_str(), textures.font, msgColor, 0.6f);
				killMessageTimer -= deltaTime;
			}
			
			// Boss Fight UI
			if (currentGameMode == GameMode::BOSS_FIGHT)
			{
				// Show Start button at top middle (same line as health bar) when waiting for match to start
				if (currentMatchState == MatchState::MATCH_WAITING && bossFightState == BossFight::BossFightState::WAITING)
				{
					// Determine host: assume lowest CID among connected players is host
					bool isHostCandidate = true;
					if (!players.empty())
					{
						int32_t minCid = cid;
						for (const auto& kv : players)
						{
							if (kv.first < minCid) minCid = kv.first;
						}
						isHostCandidate = (cid == minCid);
					}
					
					if (isHostCandidate)
					{
						// Button position: top middle, same Y as health bar (0.02 = 2% from top)
						float centerX = 0.5f * renderer.windowW;
						float buttonY = 0.02f * renderer.windowH;  // Same Y as health bar
						float buttonW = 250.0f;
						float buttonH = 50.0f;
						float buttonX = centerX - buttonW * 0.5f;

						// Check hover/pressed
						glm::vec2 mousePos = platform::getRelMousePosition();
						bool mouseOver = (mousePos.x >= buttonX && mousePos.x <= buttonX + buttonW &&
										  mousePos.y >= buttonY && mousePos.y <= buttonY + buttonH);
						bool mousePressed = platform::isLMouseHeld();

						// Colors: normal/hover/pressed
						glm::vec4 btnNormal = glm::vec4(0.20f, 0.60f, 0.20f, 0.90f);
						glm::vec4 btnHover  = glm::vec4(0.30f, 0.70f, 0.30f, 0.95f);
						glm::vec4 btnPress  = glm::vec4(0.15f, 0.50f, 0.15f, 0.95f);
						glm::vec4 buttonColor = mouseOver ? (mousePressed ? btnPress : btnHover) : btnNormal;

						// Border then background
						auto borderBox = Ui::Box()
							.xLeft(buttonX - 2.0f).yTop(buttonY - 2.0f)
							.xDimensionPixels(buttonW + 4.0f).yDimensionPixels(buttonH + 4.0f);
						renderer.renderRectangle(borderBox, {0.0f, 0.0f, 0.0f, 0.80f});

						auto buttonBox = Ui::Box()
							.xLeft(buttonX).yTop(buttonY)
							.xDimensionPixels(buttonW).yDimensionPixels(buttonH);
						renderer.renderRectangle(buttonBox, buttonColor);

						// Centered text using gl2d metrics
						const char* startText = "START";
						float textScale = 0.9f;
						auto textSize = renderer.getTextSize(startText, textures.font, textScale);
						glm::vec2 startTextPos = {
							centerX - textSize.x * 0.5f,
							buttonY + (buttonH + textSize.y) * 0.5f
						};
						renderer.renderText(startTextPos, startText, textures.font,
							glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), textScale, 4.f, 3.f, false);

						// Activate on click or Enter/Controller Start
						bool activate = (mouseOver && platform::isLMousePressed())
							|| platform::isKeyPressedOn(platform::Button::Enter)
							|| platform::getControllerButtons().buttons[platform::ControllerButtons::Start].pressed;
						if (activate)
						{
							if (joined && server)
							{
								Packet p;
								p.header = headerBossFightStartRequest; // server handles manual start for boss fight
								p.cid = cid;
								sendPacket(server, p, nullptr, 0, true, 0);
								std::cout << "Sent boss fight start request to server (CID: " << cid << ")" << std::endl;
							}
						}
					}
				}
				
				// Boss status display (when boss is active)
				if (clientBoss.isAlive)
				{
					char bossStatusText[256];
					snprintf(bossStatusText, sizeof(bossStatusText), 
						"BOSS | HP: %.0f/%.0f | Phase: %d",
						clientBoss.health, clientBoss.maxHealth,
						static_cast<int>(clientBoss.currentPhase) + 1);
					
					glm::vec2 statusPos = glm::vec2(0.05f * renderer.windowW, 50.0f);
					renderer.renderText(statusPos, bossStatusText, textures.font, 
						glm::vec4(1.0f, 0.2f, 0.2f, 1.0f), 0.8f, 4.f, 3.f, false);
				}
				
				// DEBUG: Boss Fight debug UI (only in debug builds or when needed)
				#ifdef _DEBUG
				// Boss status at top
				char bossStatusText[256];
				if (clientBoss.isAlive)
				{
					snprintf(bossStatusText, sizeof(bossStatusText), 
						"[DEBUG] Boss Alive | Pos: (%.1f, %.1f) | HP: %.0f/%.0f | Phase: %d",
						clientBoss.position.x, clientBoss.position.y,
						clientBoss.health, clientBoss.maxHealth,
						static_cast<int>(clientBoss.currentPhase) + 1);
				}
				else
				{
					snprintf(bossStatusText, sizeof(bossStatusText), "[DEBUG] Boss Not Spawned");
				}
				
				glm::vec2 debugPos = glm::vec2(0.05f * renderer.windowW, 0.03f * renderer.windowH);
				renderer.renderText(debugPos, bossStatusText, textures.font, 
					glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), 0.5f, 4.f, 3.f, false);
				
				// Player position debug
				char playerPosText[128];
				snprintf(playerPosText, sizeof(playerPosText), 
					"[DEBUG] Player Pos: (%.1f, %.1f) | Camera: (%.1f, %.1f)",
					player.pos.x, player.pos.y,
					c.position.x, c.position.y);
				glm::vec2 playerDebugPos = glm::vec2(0.05f * renderer.windowW, 0.06f * renderer.windowH);
				renderer.renderText(playerDebugPos, playerPosText, textures.font,
					glm::vec4(0.5f, 1.0f, 0.5f, 1.0f), 0.5f, 4.f, 3.f, false);
				
				// Proximity damage radius slider
				static float proximityRadius = 3.0f;
				float sliderX = 0.70f * renderer.windowW;
				float sliderY = 0.03f * renderer.windowH;
				float sliderW = 150.0f;
				float sliderH = 20.0f;
				
				// Slider label
				char sliderLabel[64];
				snprintf(sliderLabel, sizeof(sliderLabel), "Damage Zone: %.1f tiles", proximityRadius);
				glm::vec2 labelPos = glm::vec2(sliderX - 150.0f, sliderY + 3.0f);
				renderer.renderText(labelPos, sliderLabel, textures.font,
					glm::vec4(1.0f, 0.8f, 0.2f, 1.0f), 0.5f, 4.f, 3.f, false);
				
				// Slider background
				auto sliderBg = Ui::Box().xLeft(sliderX).yTop(sliderY).xDimensionPixels(sliderW).yDimensionPixels(sliderH);
				renderer.renderRectangle(sliderBg, glm::vec4(0.3f, 0.3f, 0.3f, 0.8f));
				
				// Slider handle
				float handleX = sliderX + (proximityRadius / 10.0f) * sliderW;
				auto sliderHandle = Ui::Box().xLeft(handleX - 5.0f).yTop(sliderY - 5.0f).xDimensionPixels(10.0f).yDimensionPixels(sliderH + 10.0f);
				renderer.renderRectangle(sliderHandle, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
				
				// Handle slider interaction
				bool sliderMouseOver = (platform::getRelMousePosition().x >= sliderX && 
										platform::getRelMousePosition().x <= sliderX + sliderW &&
										platform::getRelMousePosition().y >= sliderY - 10.0f && 
										platform::getRelMousePosition().y <= sliderY + sliderH + 10.0f);
				
				if (sliderMouseOver && platform::isLMouseHeld())
				{
					float mouseX = platform::getRelMousePosition().x;
					float normalizedX = (mouseX - sliderX) / sliderW;
					normalizedX = std::max(0.0f, std::min(1.0f, normalizedX));
					proximityRadius = normalizedX * 10.0f;
				}
				
				// Respawn button
				float btnX = 0.40f * renderer.windowW;
				float btnY = 0.09f * renderer.windowH;
				float btnW = 200.0f;
				float btnH = 35.0f;
				
				auto btnBox = Ui::Box().xLeft(btnX).yTop(btnY).xDimensionPixels(btnW).yDimensionPixels(btnH);
				
				// Check if mouse is over button
				bool mouseOver = (platform::getRelMousePosition().x >= btnX && 
								  platform::getRelMousePosition().x <= btnX + btnW &&
								  platform::getRelMousePosition().y >= btnY && 
								  platform::getRelMousePosition().y <= btnY + btnH);
				
				glm::vec4 btnColor = mouseOver ? glm::vec4(0.8f, 0.2f, 0.2f, 0.9f) : glm::vec4(0.6f, 0.1f, 0.1f, 0.8f);
				renderer.renderRectangle(btnBox, btnColor);
				
				// Button text
				glm::vec2 btnTextPos = glm::vec2(btnX + 20.0f, btnY + 10.0f);
				renderer.renderText(btnTextPos, "Respawn Boss (Click)", textures.font,
					glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.6f, 4.f, 3.f, false);
				
				// Handle button click
				if (mouseOver && platform::isLMousePressed())
				{
					// Send request to server to respawn boss at player position
					Packet respawnPacket;
					respawnPacket.cid = cid;
					respawnPacket.header = headerBossFightDebugRespawnBoss;
					
					BossFightDebugRespawnBossData respawnRequest;
					respawnRequest.posX = player.pos.x;
					respawnRequest.posY = player.pos.y;
					
					sendPacket(server, respawnPacket, (char*)&respawnRequest, sizeof(respawnRequest), true, 1);
					
					std::cout << "[DEBUG] Requested boss respawn at player position (" 
							  << player.pos.x << ", " << player.pos.y << ")" << std::endl;
				}
				#endif // _DEBUG
			}
			
			// Display match end screen (Deathmatch only)
			if (matchEnded && currentGameMode == GameMode::DEATHMATCH)
			{
				// Semi-transparent overlay
				auto overlayPos = Ui::Box().xLeftPerc(0.0).yTopPerc(0.0).xDimensionPercentage(1.0).yDimensionPercentage(1.0);
				renderer.renderRectangle(overlayPos, {0.0f, 0.0f, 0.0f, 0.7f});
				
				// Victory text
				glm::vec2 victoryPos = glm::vec2(0.35f * renderer.windowW, 0.35f * renderer.windowH);
				renderer.renderText(victoryPos, "MATCH ENDED!", textures.font, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), 1.0f);
				
				// Winner info
				char winnerText[128];
				snprintf(winnerText, sizeof(winnerText), "Winner: %s (%d kills)", matchWinnerName, matchWinnerKills);
				glm::vec2 winnerPos = glm::vec2(0.30f * renderer.windowW, 0.45f * renderer.windowH);
				renderer.renderText(winnerPos, winnerText, textures.font, Colors_White, 0.7f);
				
				// Instructions
				glm::vec2 instructPos = glm::vec2(0.35f * renderer.windowW, 0.55f * renderer.windowH);
				renderer.renderText(instructPos, "Press ESC to leave", textures.font, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), 0.5f);
			}
			
			// Display death screen (Horde Defense only)
			if (player.life <= 0 && currentGameMode == GameMode::HORDE_DEFENSE)
			{
				static bool wasDeadLastFrame = false;
				if (!wasDeadLastFrame)
				{
					std::cout << "[ClientDeath] Entering death screen. HP: " << player.life << std::endl;
					wasDeadLastFrame = true;
				}
				
				// Semi-transparent red overlay
				auto overlayPos = Ui::Box().xLeftPerc(0.0).yTopPerc(0.0).xDimensionPercentage(1.0).yDimensionPercentage(1.0);
				renderer.renderRectangle(overlayPos, {0.3f, 0.0f, 0.0f, 0.6f});
				
				// Death text
				glm::vec2 deathPos = glm::vec2(0.40f * renderer.windowW, 0.35f * renderer.windowH);
				renderer.renderText(deathPos, "YOU DIED", textures.font, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f), 1.2f);
				
				// Status message based on wave state
				glm::vec2 statusPos = glm::vec2(0.30f * renderer.windowW, 0.45f * renderer.windowH);
				if (hordeState == HordeDefense::HordeDefenseState::BUYING_PHASE)
				{
					renderer.renderText(statusPos, "You will respawn at the start of the next wave", 
						textures.font, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.6f);
				}
				else
				{
					char statusText[128];
					snprintf(statusText, sizeof(statusText), "Spectating... You will respawn next wave");
					renderer.renderText(statusPos, statusText, textures.font, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.6f);
					
					// Show current wave info
					glm::vec2 wavePos = glm::vec2(0.35f * renderer.windowW, 0.52f * renderer.windowH);
					char waveText[64];
					snprintf(waveText, sizeof(waveText), "Wave %d/%d in progress", currentWave, totalWaves);
					renderer.renderText(wavePos, waveText, textures.font, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), 0.5f);
				}
				
				// Instructions
				glm::vec2 instructPos = glm::vec2(0.35f * renderer.windowW, 0.60f * renderer.windowH);
				renderer.renderText(instructPos, "Press ESC to leave match", textures.font, glm::vec4(0.7f, 0.7f, 0.7f, 1.0f), 0.5f);
			}
			else if (currentGameMode == GameMode::HORDE_DEFENSE)
			{
				static bool wasDeadLastFrame = false;
				if (wasDeadLastFrame)
				{
					std::cout << "[ClientRespawn] Exiting death screen. HP: " << player.life << std::endl;
					wasDeadLastFrame = false;
				}
			}

			renderer.currentCamera = c;

		

		}
	#pragma endregion


	}

	}

	

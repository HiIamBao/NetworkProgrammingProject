#include "serverClient.h"
#include <unordered_map>
#include <map>
#include "packet.h"
#include "Phisics.h"
#include "GameRoom.h"
#include "HordeDefenseManager.h"
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iostream>
#include <mutex>

struct Client
{
	ENetPeer *peer = {};
	phisics::Entity entityData = {};
	bool changed = 1;
	int kills = 0;
	int deaths = 0;
	//char clientName[56] = {};
};

// Per-server instance state
struct ServerInstance {
	std::vector<glm::ivec2> itemSpawnPosition;
	std::vector<phisics::Item> items;
	std::unordered_map<int32_t, Client> connections;
	int pids;
	bool changedData;
	std::atomic<bool> serverOpen;
	
	// Game mode state
	GameMode gameMode;
	MatchState matchState;
	float matchStartTime;
	float matchDuration;  // in seconds, 0 = infinite
	int scoreLimit;       // 0 = no limit
	int32_t leadingPlayerCid;
	int leadingPlayerKills;
	
	// Horde Defense manager (only used for HORDE_DEFENSE mode)
	HordeDefenseManager* hordeDefenseManager;
	
	ServerInstance() : pids(1), changedData(false), serverOpen(false), 
	                   gameMode(GameMode::DEATHMATCH), matchState(MatchState::MATCH_WAITING),
	                   matchStartTime(0), matchDuration(300), scoreLimit(25),
	                   leadingPlayerCid(0), leadingPlayerKills(0),
	                   hordeDefenseManager(nullptr) {
		itemSpawnPosition = {
			{22,12}, {44,17}, {31,32}, {16,45}, {39,28},
			{11,23}, {25,5}, {27,46}, {22,27}
		};
	}
	
	~ServerInstance() {
		if (hordeDefenseManager) {
			delete hordeDefenseManager;
			hordeDefenseManager = nullptr;
		}
	}
};

// Global map to track server instances by port
static std::map<int, ServerInstance*> serverInstances;
static std::mutex instancesMutex;

constexpr int maxItems = 4;

void broadCast(ServerInstance* instance, Packet p, void *data, size_t size, ENetPeer *peerToIgnore, bool reliable, int channel)
{
	for (auto it = instance->connections.begin(); it != instance->connections.end(); it++)
	{
		if (!peerToIgnore || (it->second.peer != peerToIgnore))
		{
			sendPacket(it->second.peer, p, (const char *)data, size, true, channel);
		}
	}
}

// Broadcast wrapper for HordeDefenseManager
void hordeDefenseBroadcast(ServerInstance* instance, Packet p, const void* data, size_t size, bool reliable)
{
	broadCast(instance, p, (void*)data, size, nullptr, reliable, 0);
}

// Send to specific player wrapper for HordeDefenseManager
void hordeDefenseSendToPlayer(ServerInstance* instance, int32_t cid, Packet p, const void* data, size_t size, bool reliable)
{
	auto it = instance->connections.find(cid);
	if (it != instance->connections.end())
	{
		sendPacket(it->second.peer, p, (const char*)data, size, reliable, 0);
	}
}


void spawnItem(ServerInstance* instance)
{
	static int itemsIds = 1;
	int i = rand() % instance->itemSpawnPosition.size();
	auto pos = instance->itemSpawnPosition[i];
	instance->itemSpawnPosition.erase(instance->itemSpawnPosition.begin() + i);

	auto item = phisics::Item(pos, itemsIds++, rand()%phisics::itemsCount + 1);

	instance->items.push_back(item);

	Packet p;
	p.cid = 0;
	p.header = headerSpawnItem;

	broadCast(instance, p, &item, sizeof(item), nullptr, true, 1);
}

bool pickupItem(ServerInstance* instance, uint32_t itemId, phisics::Item &item)
{
	auto findPos = std::find_if(instance->items.begin(), instance->items.end(), [itemId](phisics::Item &i) { return i.itemId == itemId; });
	item = {};

	if (findPos == instance->items.end())
	{
		return false;
	}

	item = *findPos;

	auto pos = findPos->pos;
	instance->itemSpawnPosition.push_back(pos);
	instance->items.erase(findPos);
	return true;
}

glm::vec3 getRandomColor()
{
	glm::vec3 colors[] = 
	{
		Colors_Blue
		,Colors_Yellow
		,Colors_Magenta
		,Colors_Turqoise
		,Colors_Orange
		,Colors_Purple
		,Colors_Gray
	};

	int index = rand() % (sizeof(colors) / sizeof(colors[0]));

	return colors[index];
}

void addConnection(ServerInstance* instance, ENetHost *server, ENetEvent &event)
{
	instance->changedData = true;
	phisics::Entity entity = {};
	glm::vec3 color = getRandomColor();
	entity.color = color;

	instance->connections.insert({instance->pids, Client{event.peer, entity}});

	Packet p;
	p.header = headerReceiveCIDAndData;
	p.cid = instance->pids;

	instance->pids++;
	//send own cid
	sendPacket(event.peer, p, (const char*)&color, sizeof(color), true, 0);

	//send other players
	for (auto it = instance->connections.begin(); it != instance->connections.end(); it++)
	{
		if (it->second.peer != event.peer)
		{
			Packet sPacket;
			sPacket.header = headerUpdateConnection;
			sPacket.cid = it->first;
			sendPacket(event.peer, sPacket, (const char *)&it->second.entityData, sizeof(phisics::Entity), true, 0);
		}
	}

	//send other items
	for (auto &it : instance->items)
	{
		Packet p;
		p.cid = 0;
		p.header = headerSpawnItem;
		sendPacket(event.peer, p, (const char *)&it, sizeof(phisics::Item), true, 1);
	}

	//broadcast data of new connection
	Packet sPacket;
	sPacket.header = headerAnounceConnection;
	sPacket.cid = p.cid;

	broadCast(instance, sPacket, &entity, sizeof(entity), event.peer, true, 0);
	
	// Register player in Horde Defense mode
	if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
	{
		instance->hordeDefenseManager->addPlayer(p.cid);
	}
	
	// Send game mode information to the newly connected player
	// (Important: Do this BEFORE the match start check, so joining players get the mode)
	if (instance->matchState == MatchState::MATCH_IN_PROGRESS)
	{
		// Match already started - send current game state to new player
		MatchStartData startData;
		startData.gameMode = static_cast<int>(instance->gameMode);
		startData.matchDuration = instance->matchDuration;
		startData.scoreLimit = instance->scoreLimit;
		
		Packet startPacket;
		startPacket.header = headerMatchStart;
		startPacket.cid = 0;
		sendPacket(event.peer, startPacket, (const char*)&startData, sizeof(startData), true, 0);
		
		std::cout << "Player joined ongoing match. Sent game mode: " << static_cast<int>(instance->gameMode) << std::endl;
		
		// If Horde Defense, also send current wave state
		if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
		{
			instance->hordeDefenseManager->sendFullStateToPlayer(p.cid, event.peer);
		}
	}
	
	// Auto-start match if this is the first player (for testing)
	// Or start when 2+ players join
	if (instance->connections.size() >= 1 && instance->matchState == MatchState::MATCH_WAITING)
	{
		instance->matchState = MatchState::MATCH_IN_PROGRESS;
		instance->matchStartTime = 0;  // Will be set properly with a timer
		
		MatchStartData startData;
		startData.gameMode = static_cast<int>(instance->gameMode);
		startData.matchDuration = instance->matchDuration;
		startData.scoreLimit = instance->scoreLimit;
		
		Packet startPacket;
		startPacket.header = headerMatchStart;
		startPacket.cid = 0;
		broadCast(instance, startPacket, &startData, sizeof(startData), nullptr, true, 0);
		
		if (instance->gameMode == GameMode::DEATHMATCH)
		{
			std::cout << "Match started! Mode: Free-for-All, Score limit: " << instance->scoreLimit << std::endl;
		}
		else if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
		{
			std::cout << "Match started! Mode: Horde Defense" << std::endl;
			instance->hordeDefenseManager->startMatch();
		}
	}

}

void removeConnection(ServerInstance* instance, ENetHost *server, ENetEvent &event)
{

	for (auto it = instance->connections.begin(); it != instance->connections.end(); it++)
	{
		if (it->second.peer == event.peer)
		{
			// Remove player from Horde Defense mode
			if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
			{
				instance->hordeDefenseManager->removePlayer(it->first);
			}

			//broadcast disconnect
			Packet sPacket;
			sPacket.header = headerAnounceDisconnect;
			sPacket.cid = it->first;

			broadCast(instance, sPacket, nullptr, 0, event.peer, true, 0);

			enet_peer_disconnect(event.peer, 0);
			instance->connections.erase(it);
			break;
		}
	}
}

void recieveData(ServerInstance* instance, ENetHost *server, ENetEvent &event)
{
	instance->changedData = true;

	Packet p;
	size_t size = 0;
	auto data = parsePacket(event, p, size);

	//validate data
	if (instance->connections[p.cid].peer != event.peer)
	{
		//std::cout << "invalid data!\n";
		return;
	}

	if (p.header == headerUpdateConnection)
	{
		// In Horde Defense mode, don't let clients overwrite server-authoritative fields
		if (instance->gameMode == GameMode::HORDE_DEFENSE)
		{
			phisics::Entity clientData = *(phisics::Entity*)(data);
			phisics::Entity& serverData = instance->connections[p.cid].entityData;
			
			// Only update client-controlled fields (position, input)
			serverData.pos = clientData.pos;
			serverData.lastPos = clientData.lastPos;
			serverData.input = clientData.input;
			serverData.moving = clientData.moving;
			serverData.movingRight = clientData.movingRight;
			
			// Server controls: life, money, upgrades, buffs
			// (Don't copy these from client)
		}
		else
		{
			// In other modes, accept full entity update from client
			instance->connections[p.cid].entityData = *(phisics::Entity*)(data);
		}
		instance->connections[p.cid].changed = true;
	}
	else if (p.header == headerSendBullet)
	{
		// Check if the player is alive before processing bullet
		auto playerIt = instance->connections.find(p.cid);
		if (playerIt != instance->connections.end())
		{
			if (playerIt->second.entityData.life <= 0)
			{
				// Dead players can't shoot
				std::cout << "Rejected bullet from dead player CID " << p.cid << std::endl;
				return;
			}
		}
		
		Packet sPacket;
		sPacket.header = headerSendBullet;
		sPacket.cid = p.cid;
		broadCast(instance, sPacket, data, size, event.peer, true, 1);

	}
	else if (p.header == headerRegisterHit)
	{
		// Player vs Player combat - ONLY in Deathmatch and Team modes
		// Skip this in Horde Defense (cooperative mode)
		if (instance->gameMode == GameMode::HORDE_DEFENSE)
		{
			// In Horde Defense, players can't damage each other
			std::cout << "Ignored PvP hit in Horde Defense mode (cooperative)" << std::endl;
			return;
		}
		
		int32_t victimCid = *(int32_t *)data;
		int32_t killerCid = p.cid;
		
		// Find victim and killer
		auto victimIt = instance->connections.find(victimCid);
		auto killerIt = instance->connections.find(killerCid);
		
		if (victimIt != instance->connections.end() && killerIt != instance->connections.end())
		{
			// Check if victim died (life <= 0)
			if (victimIt->second.entityData.life <= 1)  // Will be 0 after this hit
			{
				// Update kill/death stats
				killerIt->second.kills++;
				killerIt->second.entityData.kills++;
				victimIt->second.deaths++;
				victimIt->second.entityData.deaths++;
				
				// Broadcast kill notification
				PlayerKillData killData;
				killData.killerCid = killerCid;
				killData.victimCid = victimCid;
				strncpy(killData.killerName, killerIt->second.entityData.name, sizeof(killData.killerName) - 1);
				strncpy(killData.victimName, victimIt->second.entityData.name, sizeof(killData.victimName) - 1);
				
				Packet killPacket;
				killPacket.header = headerPlayerKill;
				killPacket.cid = 0;
				broadCast(instance, killPacket, &killData, sizeof(killData), nullptr, true, 0);
				
				// Update leading player
				if (killerIt->second.kills > instance->leadingPlayerKills)
				{
					instance->leadingPlayerKills = killerIt->second.kills;
					instance->leadingPlayerCid = killerCid;
				}
				
				// Check for victory condition (score limit reached)
				if (instance->scoreLimit > 0 && killerIt->second.kills >= instance->scoreLimit)
				{
					// Match ended! Broadcast match end
					MatchEndData endData;
					endData.winnerCid = killerCid;
					strncpy(endData.winnerName, killerIt->second.entityData.name, sizeof(endData.winnerName) - 1);
					endData.winnerKills = killerIt->second.kills;
					endData.winnerDeaths = killerIt->second.deaths;
					endData.totalPlayers = instance->connections.size();
					
					Packet endPacket;
					endPacket.header = headerMatchEnd;
					endPacket.cid = 0;
					broadCast(instance, endPacket, &endData, sizeof(endData), nullptr, true, 0);
					
					instance->matchState = MatchState::MATCH_ENDED;
				}
			}
		}
		
		// Broadcast hit to all clients
		Packet sPacket;
		sPacket.header = headerRegisterHit;
		sPacket.cid = victimCid;
		broadCast(instance, sPacket, nullptr, 0, event.peer, true, 1);
	}
	// ========================================================================
	// HORDE DEFENSE PACKET HANDLERS
	// ========================================================================
	else if (p.header == headerHordeBuyUpgrade)
	{
		if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
		{
			HordeBuyUpgradeData* buyData = (HordeBuyUpgradeData*)data;
			auto playerIt = instance->connections.find(p.cid);
			
			if (playerIt != instance->connections.end())
			{
				HordeBuyUpgradeResponse response;
				bool success = instance->hordeDefenseManager->buyUpgrade(
					p.cid, 
					playerIt->second.entityData, 
					static_cast<HordeDefense::UpgradeType>(buyData->upgradeType),
					response
				);
						// Send response to requesting player
			Packet respPacket;
			respPacket.header = headerHordeBuyUpgradeResponse;
			respPacket.cid = p.cid;
			sendPacket(event.peer, respPacket, (const char*)&response, sizeof(response), true, 0);
			
			// If successful, broadcast player stat update to ALL clients
			if (success)
			{
				// Broadcast full stats update so clients can update their UI
				HordePlayerStatsUpdate statsUpdate;
				statsUpdate.cid = p.cid;
				statsUpdate.damageLevel = playerIt->second.entityData.damageUpgradeLevel;
				statsUpdate.fireRateLevel = playerIt->second.entityData.fireRateUpgradeLevel;
				statsUpdate.healthLevel = playerIt->second.entityData.healthUpgradeLevel;
				statsUpdate.speedLevel = playerIt->second.entityData.speedUpgradeLevel;
				statsUpdate.bulletSpeedLevel = playerIt->second.entityData.bulletSpeedUpgradeLevel;
				statsUpdate.speedBoostWaves = playerIt->second.entityData.speedBoostWaves;
				statsUpdate.damageBoostWaves = playerIt->second.entityData.damageBoostWaves;
				statsUpdate.multiShotWaves = playerIt->second.entityData.multiShotWaves;
				statsUpdate.shieldHealth = playerIt->second.entityData.shieldHealth;
				
				Packet statsPacket;
				statsPacket.header = headerHordePlayerStatsUpdate;
				statsPacket.cid = 0;
				broadCast(instance, statsPacket, &statsUpdate, sizeof(statsUpdate), nullptr, true, 0);
				
				// IMMEDIATELY broadcast updated entity (including HP/maxLife) to all clients
				Packet entityPacket;
				entityPacket.header = headerUpdateConnection;
				entityPacket.cid = p.cid;
				broadCast(instance, entityPacket, &playerIt->second.entityData, sizeof(phisics::Entity), nullptr, true, 0);
				
				std::cout << "[HordeDefense] Broadcasted upgrade for player " << p.cid 
				          << " - Health Level: " << statsUpdate.healthLevel 
				          << ", MaxHP: " << playerIt->second.entityData.maxLife 
				          << ", CurrentHP: " << playerIt->second.entityData.life << std::endl;
				
				// Also mark entity for next frame broadcast
				playerIt->second.changed = true;
				instance->changedData = true;
			}
			}
		}
	}
	else if (p.header == headerHordeBuyItem)
	{
		if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
		{
			HordeBuyItemData* buyData = (HordeBuyItemData*)data;
			auto playerIt = instance->connections.find(p.cid);
			
			if (playerIt != instance->connections.end())
			{
				HordeBuyItemResponse response;
				bool success = instance->hordeDefenseManager->buyItem(
					p.cid,
					playerIt->second.entityData,
					static_cast<HordeDefense::ShopItemType>(buyData->itemType),
					response
				);
						// Send response to requesting player
			Packet respPacket;
			respPacket.header = headerHordeBuyItemResponse;
			respPacket.cid = p.cid;
			sendPacket(event.peer, respPacket, (const char*)&response, sizeof(response), true, 0);
			
			// If successful, immediately broadcast updated entity to all clients
			if (success)
			{
				// IMMEDIATELY broadcast updated entity (including HP/maxLife) to all clients
				Packet entityPacket;
				entityPacket.header = headerUpdateConnection;
				entityPacket.cid = p.cid;
				broadCast(instance, entityPacket, &playerIt->second.entityData, sizeof(phisics::Entity), nullptr, true, 0);
				
				std::cout << "[HordeDefense] Broadcasted item purchase for player " << p.cid 
				          << " - HP: " << playerIt->second.entityData.life 
				          << "/" << playerIt->second.entityData.maxLife << std::endl;
				
				// Also mark entity for next frame broadcast
				playerIt->second.changed = true;
				instance->changedData = true;
			}
			}
		}
	}
	// TODO: Bullet-enemy collision detection
	// In Horde Defense mode, bullets need to be checked against enemies on the server
	// This will require intercepting headerSendBullet and checking collisions
	else if (p.header == headerPickupItem)
	{
		Packet sPacket;
		sPacket.header = headerPickupItem;
		sPacket.cid = p.cid;
		phisics::Item item = {};

		if (pickupItem(instance, *(uint32_t *)data, item))
		{
			broadCast(instance, sPacket, &item, sizeof(item), nullptr, true, 1);
		}
	}
	else if (p.header == headerHordeBulletHitEnemy)
	{
		if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
		{
			HordeBulletHitEnemyData* hitData = (HordeBulletHitEnemyData*)data;
			auto playerIt = instance->connections.find(p.cid);
			
			if (playerIt != instance->connections.end())
			{
				// Check if player is alive - dead players can't damage enemies
				if (playerIt->second.entityData.life <= 0)
				{
					std::cout << "Rejected bullet damage from dead player CID " << p.cid << std::endl;
					return;
				}
						// Calculate actual damage based on player's upgrades and buffs
			int baseDamage = 10;  // Base bullet damage
			phisics::Entity& playerEntity = playerIt->second.entityData;
			
			// Apply damage upgrade multiplier
			float damageMultiplier = 1.0f + (playerEntity.damageUpgradeLevel * 0.25f);  // +25% per level
			
			// Apply damage boost buff if active (wave-based)
			if (playerEntity.damageBoostWaves > 0)
			{
				damageMultiplier += 1.0f;  // +100% from damage amplifier
			}
				
				int actualDamage = (int)(baseDamage * damageMultiplier);
				
				// Apply damage to enemy
				instance->hordeDefenseManager->damageEnemy(hitData->enemyId, actualDamage, p.cid);
				
				// Server will broadcast enemy update/death automatically via its update loop
			}
		}
	}

	enet_packet_destroy(event.packet);
}

void closeServer()
{
	// Close all server instances
	std::lock_guard<std::mutex> lock(instancesMutex);
	for (auto& pair : serverInstances) {
		if (pair.second) {
			pair.second->serverOpen = false;
		}
	}
}

// Close specific server instance by port
void closeServerByPort(int port)
{
	std::lock_guard<std::mutex> lock(instancesMutex);
	auto it = serverInstances.find(port);
	if (it != serverInstances.end() && it->second) {
		it->second->serverOpen = false;
	}
}

// Check if server is currently running on specific port
bool isServerRunning(int port)
{
	std::lock_guard<std::mutex> lock(instancesMutex);
	auto it = serverInstances.find(port);
	return (it != serverInstances.end() && it->second && it->second->serverOpen.load());
}

// Check if any server is running
bool isServerRunning()
{
	std::lock_guard<std::mutex> lock(instancesMutex);
	for (const auto& pair : serverInstances) {
		if (pair.second && pair.second->serverOpen.load()) {
			return true;
		}
	}
	return false;
}

// Reset server state variables - no longer needed per instance
void resetServerState()
{
	// This function is kept for backwards compatibility but does nothing
	// State is now managed per-instance in ServerInstance
}

void serverFunction(int port, int gameMode, int mapId)
{
	// Check if server is already running on this port
	{
		std::lock_guard<std::mutex> lock(instancesMutex);
		auto it = serverInstances.find(port);
		if (it != serverInstances.end() && it->second && it->second->serverOpen.load())
		{
			std::cout << "Server is already running on port " << port << "!" << std::endl;
			return;
		}
	}

	std::srand(std::time(0));
	
	// Create new server instance
	ServerInstance* instance = new ServerInstance();
	instance->serverOpen = true;
	
	// Set game mode from parameter
	instance->gameMode = static_cast<GameMode>(gameMode);
	std::cout << "Server starting on port " << port << " with GameMode: " << gameMode << ", Map: " << mapId << std::endl;
	
	// Initialize Horde Defense manager if needed
	if (instance->gameMode == GameMode::HORDE_DEFENSE)
	{
		instance->hordeDefenseManager = new HordeDefenseManager();
		instance->hordeDefenseManager->initialize();
		
		// Set network callbacks
		instance->hordeDefenseManager->setBroadcastCallback(
			[instance](Packet p, const void* data, size_t size, bool reliable) {
				hordeDefenseBroadcast(instance, p, data, size, reliable);
			}
		);
		
		instance->hordeDefenseManager->setSendToPlayerCallback(
			[instance](int32_t cid, Packet p, const void* data, size_t size, bool reliable) {
				hordeDefenseSendToPlayer(instance, cid, p, data, size, reliable);
			}
		);
		
		instance->hordeDefenseManager->setPlayerDamageCallback(
			[instance](int32_t cid, int damage) {
				auto playerIt = instance->connections.find(cid);
				if (playerIt != instance->connections.end()) {
					phisics::Entity& player = playerIt->second.entityData;
					
					// Only damage alive players
					if (player.life > 0) {
						// Apply shield damage first
						if (player.shieldHealth > 0) {
							int shieldDamage = std::min((int)player.shieldHealth, damage);
							player.shieldHealth -= shieldDamage;
							damage -= shieldDamage;
						}
						
						// Apply remaining damage to health
						if (damage > 0) {
							player.life -= damage;
							if (player.life < 0) player.life = 0;
						
							std::cout << "[ServerDamage] Player " << cid << " took " << damage << " damage. HP: " << player.life << std::endl;
									
							// Mark for broadcast
							playerIt->second.changed = true;
							instance->changedData = true;
						}
					}
				}
			}
		);
		
		std::cout << "Horde Defense mode initialized." << std::endl;
	}
	
	// Register instance
	{
		std::lock_guard<std::mutex> lock(instancesMutex);
		serverInstances[port] = instance;
	}

	ENetAddress adress;
	adress.host = ENET_HOST_ANY;
	adress.port = port;
	ENetEvent event;

	//first param adress, players limit, channels, bandwith limit
	ENetHost *server = enet_host_create(&adress, 32, SERVER_CHANNELS, 0, 0);

	if (!server)
	{
		std::cout << "Failed to create server! Port " << port << " may be in use." << std::endl;
		instance->serverOpen = false;
		
		// Cleanup instance
		{
			std::lock_guard<std::mutex> lock(instancesMutex);
			serverInstances.erase(port);
		}
		delete instance;
		return;
	}
	
	std::cout << "Server started successfully on port " << port << std::endl;


	while (instance->serverOpen)
	{
		int counter = 0;
		constexpr int maxCounter = 10;

		while (enet_host_service(server, &event, 0) > 0 && counter < maxCounter && instance->serverOpen)
		{
			counter++;
			switch (event.type)
			{
				case ENET_EVENT_TYPE_CONNECT:
				{
					addConnection(instance, server, event);

					break;
				}
				case ENET_EVENT_TYPE_RECEIVE:
				{
					recieveData(instance, server, event);

					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT:
				{
					removeConnection(instance, server, event);
					break;
				}
			}
		}

		if (instance->changedData)
		{
			for (auto p = instance->connections.begin(); p != instance->connections.end(); p++)
			{
				
				if (!p->second.changed)
				{
					continue;
				}
				
				p->second.changed = false;

				Packet sPacket;
				sPacket.header = headerUpdateConnection;
				sPacket.cid = p->first;
				broadCast(instance, sPacket, &p->second.entityData, sizeof(phisics::Entity), p->second.peer, false, 0);
			}
			
		}

		instance->changedData = false;
		
		float deltaTime = 0.f;
		{
			static auto stop = std::chrono::high_resolution_clock::now();
			auto start = std::chrono::high_resolution_clock::now();

			deltaTime = (std::chrono::duration_cast<std::chrono::microseconds>(start - stop)).count() / 1000000.0f;
			stop = std::chrono::high_resolution_clock::now();
		}

	#pragma region Horde Defense Update
		if (instance->gameMode == GameMode::HORDE_DEFENSE && instance->hordeDefenseManager)
		{
			// Update Horde Defense game logic
			std::map<int32_t, phisics::Entity> playerEntities;
			for (const auto& conn : instance->connections)
			{
				playerEntities[conn.first] = conn.second.entityData;
			}
			
			instance->hordeDefenseManager->update(deltaTime);
			instance->hordeDefenseManager->updateEnemies(deltaTime, playerEntities);
			
			// Decrement wave-based buffs when wave COMPLETES (not when it starts)
			// This way, buying an item during buy phase makes it last through the entire next wave
			static HordeDefense::HordeDefenseState lastHordeState = HordeDefense::HordeDefenseState::WAITING;
			HordeDefense::HordeDefenseState currentHordeState = instance->hordeDefenseManager->getState();
			
			// When transitioning from WAVE_ACTIVE to BUYING_PHASE (wave just completed), decrement buffs
			if (lastHordeState == HordeDefense::HordeDefenseState::WAVE_ACTIVE && 
			    currentHordeState == HordeDefense::HordeDefenseState::BUYING_PHASE)
			{
				instance->hordeDefenseManager->decrementWaveBasedBuffs(playerEntities);
				
				// Apply decremented buffs back to connections
				for (auto& [cid, entity] : playerEntities) {
					auto it = instance->connections.find(cid);
					if (it != instance->connections.end()) {
						it->second.entityData.speedBoostWaves = entity.speedBoostWaves;
						it->second.entityData.damageBoostWaves = entity.damageBoostWaves;
						it->second.entityData.multiShotWaves = entity.multiShotWaves;
						it->second.changed = true;
					}
				}
				instance->changedData = true;
				
				std::cout << "[HordeDefense] Wave completed - decremented wave-based buffs" << std::endl;
			}
			lastHordeState = currentHordeState;
			
			// Handle player respawning (after wave complete)
			for (auto& conn : instance->connections)
			{
				// If player needs respawn (wave just ended and they were dead)
				if (instance->hordeDefenseManager->needsRespawn(conn.first))
				{
					std::cout << "[HordeDefense] Player " << conn.first << " needs respawn! Current HP: " << conn.second.entityData.life << std::endl;
					
					// Actually respawn the player (this will restore HP regardless of current value)
					instance->hordeDefenseManager->respawnPlayer(conn.first, conn.second.entityData);
					instance->hordeDefenseManager->markPlayerRespawned(conn.first);
					
					std::cout << "[HordeDefense] Player " << conn.first << " HP after respawn: " << conn.second.entityData.life << std::endl;
					
					// Immediately broadcast the respawn to all clients (critical for UI update)
					Packet sPacket;
					sPacket.header = headerUpdateConnection;
					sPacket.cid = conn.first;
					broadCast(instance, sPacket, &conn.second.entityData, sizeof(phisics::Entity), nullptr, true, 0);
					std::cout << "[HordeDefense] Broadcasted respawn update for player " << conn.first << std::endl;
					
					conn.second.changed = false;  // Already broadcast, don't broadcast again
					std::cout << "[HordeDefense] Player " << conn.first << " respawned for new wave (HP: " << conn.second.entityData.life << ")" << std::endl;
				}
			}
			
			// Check for NEW player deaths (alive flag is true AND HP just dropped to 0)
			// BUT ONLY during wave phases (not during buying phase when players respawn)
			for (auto& conn : instance->connections)
			{
				bool hasHP0 = (conn.second.entityData.life <= 0);
				bool flagAlive = instance->hordeDefenseManager->isPlayerAlive(conn.first);
				bool notWaitingRespawn = !instance->hordeDefenseManager->needsRespawn(conn.first);
				
				if (hasHP0 && flagAlive && notWaitingRespawn)
				{
					// Player just died during wave - mark them as dead
					instance->hordeDefenseManager->markPlayerDead(conn.first);				std::cout << "[HordeDefense] Player " << conn.first << " died! (HP: 0)" << std::endl;
				
				// Check if all players are dead (game over)
				if (instance->hordeDefenseManager->allPlayersDead(playerEntities))
				{
					std::cout << "[HordeDefense] All players dead! Game Over!" << std::endl;
					// The HordeDefenseManager will handle game over state
				}
			}
		}
		// Note: Buff timers are now wave-based and decremented when wave completes
	}
	#pragma endregion

	#pragma region items
		// Only spawn items in non-Horde Defense modes
		if (instance->gameMode != GameMode::HORDE_DEFENSE)
		{

			static float spawnTime = 5.f;
			
			if (instance->items.size() < maxItems)
			{
				spawnTime -= deltaTime;

				if (spawnTime <= 0.f)
				{
					spawnTime = rand() % 5 + 3;

					spawnItem(instance);
				}

			}



		}
	#pragma endregion



	}
	
	std::cout << "Server on port " << port << " shutting down..." << std::endl;
	
	// Cleanup: disconnect all clients
	for (auto& conn : instance->connections) {
		enet_peer_disconnect(conn.second.peer, 0);
	}
	
	// Flush any remaining packets
	ENetEvent cleanupEvent;
	while (enet_host_service(server, &cleanupEvent, 100) > 0) {
		if (cleanupEvent.type == ENET_EVENT_TYPE_DISCONNECT) {
			// Client disconnected
		}
	}
	
	enet_host_destroy(server);
	
	// Cleanup instance
	{
		std::lock_guard<std::mutex> lock(instancesMutex);
		serverInstances.erase(port);
	}
	delete instance;
	
	std::cout << "Server on port " << port << " stopped." << std::endl;
}
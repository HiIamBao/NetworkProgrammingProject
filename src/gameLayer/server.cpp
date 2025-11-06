#include "serverClient.h"
#include <unordered_map>
#include <map>
#include "packet.h"
#include "Phisics.h"
#include "GameRoom.h"
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
	
	ServerInstance() : pids(1), changedData(false), serverOpen(false), 
	                   gameMode(GameMode::DEATHMATCH), matchState(MatchState::MATCH_WAITING),
	                   matchStartTime(0), matchDuration(300), scoreLimit(25),
	                   leadingPlayerCid(0), leadingPlayerKills(0) {
		itemSpawnPosition = {
			{22,12}, {44,17}, {31,32}, {16,45}, {39,28},
			{11,23}, {25,5}, {27,46}, {22,27}
		};
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
		
		std::cout << "Match started! Mode: Free-for-All, Score limit: " << instance->scoreLimit << std::endl;
	}

}

void removeConnection(ServerInstance* instance, ENetHost *server, ENetEvent &event)
{

	for (auto it = instance->connections.begin(); it != instance->connections.end(); it++)
	{
		if (it->second.peer == event.peer)
		{

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
		instance->connections[p.cid].entityData = *(phisics::Entity*)(data);
		instance->connections[p.cid].changed = true;
	}
	else if (p.header == headerSendBullet)
	{
		Packet sPacket;
		sPacket.header = headerSendBullet;
		sPacket.cid = p.cid;
		broadCast(instance, sPacket, data, size, event.peer, true, 1);

	}
	else if (p.header == headerRegisterHit)
	{
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

void serverFunction(int port)
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

	#pragma region items
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
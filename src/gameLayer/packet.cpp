#include "packet.h"
#include <vector>
#include <algorithm>
#include <cstring>
#undef max
#undef min


//char *dataPool;
//size_t dataPoolSize;
//todo custom allocator
//void resize(size_t newSize)
//{
//	if (newSize > dataPoolSize)
//	{
//		delete[] dataPool;
//		dataPool = new char[newSize];
//		dataPoolSize = newSize;
//	}
//}


void sendPacket(ENetPeer *to, Packet p, const char *data, size_t size, bool reliable, int channel)
{

	//resize(size + sizeof(Packet));

	char *dataPool = new char[size + sizeof(Packet)];
	//size_t dataPoolSize;


	memcpy(dataPool, &p, sizeof(Packet));

	if (data && size)
	{
		memcpy(dataPool + sizeof(Packet), data, size);
	}

	size_t flag = 0;

	if (reliable)
	{
		flag = ENET_PACKET_FLAG_RELIABLE;
	}
	else
	{
		flag = ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT;// | ENET_PACKET_FLAG_UNSEQUENCED;
	}

	ENetPacket *packet = enet_packet_create(dataPool, size + sizeof(Packet), flag);
	enet_peer_send(to, channel, packet);

	delete[] dataPool;

}

char *parsePacket(ENetEvent &event, Packet &p, size_t &dataSize)
{
	size_t size = event.packet->dataLength;
	void *data = event.packet->data;
	dataSize = std::max(size_t(0), size - sizeof(Packet));

	memcpy(&p, data, sizeof(Packet));

	if (size <= sizeof(Packet))
	{
		return nullptr;
	}
	else
	{
		return (char *)data + sizeof(Packet);
	}

}

// ============================================================================
// HORDE DEFENSE - Helper Function Implementations
// ============================================================================

void sendHordeStateUpdate(ENetPeer* peer, const HordeStateUpdateData& data, bool reliable)
{
	Packet p;
	p.header = headerHordeStateUpdate;
	p.cid = 0;
	sendPacket(peer, p, (const char*)&data, sizeof(data), reliable, 0);
}

void sendHordeEnemySpawn(ENetPeer* peer, const HordeEnemySpawnData& data, bool reliable)
{
	Packet p;
	p.header = headerHordeSpawnEnemy;
	p.cid = 0;
	sendPacket(peer, p, (const char*)&data, sizeof(data), reliable, 0);
}

void sendHordeEnemyUpdate(ENetPeer* peer, const HordeEnemyUpdateData* enemies, int count, bool reliable)
{
	Packet p;
	p.header = headerHordeEnemyUpdate;
	p.cid = 0;
	size_t dataSize = sizeof(HordeEnemyUpdateData) * count;
	sendPacket(peer, p, (const char*)enemies, dataSize, reliable, 0);
}

void sendHordeEnemyDeath(ENetPeer* peer, const HordeEnemyDeathData& data, bool reliable)
{
	Packet p;
	p.header = headerHordeEnemyDeath;
	p.cid = 0;
	sendPacket(peer, p, (const char*)&data, sizeof(data), reliable, 0);
}

void sendHordeWaveStart(ENetPeer* peer, const HordeWaveStartData& data, bool reliable)
{
	Packet p;
	p.header = headerHordeWaveStart;
	p.cid = 0;
	sendPacket(peer, p, (const char*)&data, sizeof(data), reliable, 0);
}

void sendHordeWaveComplete(ENetPeer* peer, const HordeWaveCompleteData& data, bool reliable)
{
	Packet p;
	p.header = headerHordeWaveComplete;
	p.cid = 0;
	sendPacket(peer, p, (const char*)&data, sizeof(data), reliable, 0);
}

void sendHordePlayerMoney(ENetPeer* peer, const HordePlayerMoneyUpdate& data, bool reliable)
{
	Packet p;
	p.header = headerHordePlayerMoneyUpdate;
	p.cid = data.cid;
	sendPacket(peer, p, (const char*)&data, sizeof(data), reliable, 0);
}

void sendHordePlayerStats(ENetPeer* peer, const HordePlayerStatsUpdate& data, bool reliable)
{
	Packet p;
	p.header = headerHordePlayerStatsUpdate;
	p.cid = data.cid;
	sendPacket(peer, p, (const char*)&data, sizeof(data), reliable, 0);
}

void sendHordePlayerRespawn(ENetPeer* peer, const HordePlayerRespawnData& data, bool reliable)
{
	Packet p;
	p.header = headerHordePlayerRespawn;
	p.cid = data.cid;
	sendPacket(peer, p, (const char*)&data, sizeof(data), reliable, 0);
}

void sendHordeMatchEnd(ENetPeer* peer, const HordeMatchEndData& data, bool reliable)
{
	Packet p;
	p.header = headerHordeMatchEnd;
	p.cid = 0;
	sendPacket(peer, p, (const char*)&data, sizeof(data), reliable, 0);
}

void sendHordeBuyUpgradeResponse(ENetPeer* peer, const HordeBuyUpgradeResponse& data, bool reliable)
{
	Packet p;
	p.header = headerHordeBuyUpgradeResponse;
	p.cid = 0;
	sendPacket(peer, p, (const char*)&data, sizeof(data), reliable, 0);
}

void sendHordeBuyItemResponse(ENetPeer* peer, const HordeBuyItemResponse& data, bool reliable)
{
	Packet p;
	p.header = headerHordeBuyItemResponse;
	p.cid = 0;
	sendPacket(peer, p, (const char*)&data, sizeof(data), reliable, 0);
}

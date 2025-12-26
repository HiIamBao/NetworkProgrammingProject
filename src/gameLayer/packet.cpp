#include "packet.h"
#include <vector>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <iomanip>
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

int seq =0;
char *parsePacket(ENetEvent &event, Packet &p, size_t &dataSize)
{
    size_t size = event.packet->dataLength;
    void *data = event.packet->data;
    dataSize = std::max(size_t(0), size - sizeof(Packet));
    memcpy(&p, data, sizeof(Packet));
    
    // Increment and reset sequence number
    seq++;
    if (seq >= 100000) {
        seq = 0;
    }
    
    // Print packet contents
    std::cout << "=== Packet #" << seq << " ===" << std::endl;
    
    // Print header name
    std::cout << "Header: ";
    switch(p.header) {
        case headerNone: std::cout << "headerNone"; break;
        case headerRegisterRequest: std::cout << "headerRegisterRequest"; break;
        case headerRegisterResponse: std::cout << "headerRegisterResponse"; break;
        case headerLoginRequest: std::cout << "headerLoginRequest"; break;
        case headerLoginResponse: std::cout << "headerLoginResponse"; break;
        case headerLogoutRequest: std::cout << "headerLogoutRequest"; break;
        case headerLogoutResponse: std::cout << "headerLogoutResponse"; break;
        case headerReceiveCIDAndData: std::cout << "headerReceiveCIDAndData"; break;
        case headerAnounceConnection: std::cout << "headerAnounceConnection"; break;
        case headerUpdateConnection: std::cout << "headerUpdateConnection"; break;
        case headerAnounceDisconnect: std::cout << "headerAnounceDisconnect"; break;
        case headerSendBullet: std::cout << "headerSendBullet"; break;
        case headerRegisterHit: std::cout << "headerRegisterHit"; break;
        case headerSpawnItem: std::cout << "headerSpawnItem"; break;
        case headerPickupItem: std::cout << "headerPickupItem"; break;
        case headerRequestAccountInfo: std::cout << "headerRequestAccountInfo"; break;
        case headerAccountInfo: std::cout << "headerAccountInfo"; break;
        case headerRequestLeaderboard: std::cout << "headerRequestLeaderboard"; break;
        case headerLeaderboard: std::cout << "headerLeaderboard"; break;
        case headerCreateRoomRequest: std::cout << "headerCreateRoomRequest"; break;
        case headerCreateRoomResponse: std::cout << "headerCreateRoomResponse"; break;
        case headerJoinRoomRequest: std::cout << "headerJoinRoomRequest"; break;
        case headerJoinRoomResponse: std::cout << "headerJoinRoomResponse"; break;
        case headerLeaveRoomRequest: std::cout << "headerLeaveRoomRequest"; break;
        case headerLeaveRoomResponse: std::cout << "headerLeaveRoomResponse"; break;
        case headerGetRoomListRequest: std::cout << "headerGetRoomListRequest"; break;
        case headerGetRoomListResponse: std::cout << "headerGetRoomListResponse"; break;
        case headerGetRoomInfoRequest: std::cout << "headerGetRoomInfoRequest"; break;
        case headerGetRoomInfoResponse: std::cout << "headerGetRoomInfoResponse"; break;
        case headerStartGameRequest: std::cout << "headerStartGameRequest"; break;
        case headerStartGameResponse: std::cout << "headerStartGameResponse"; break;
        case headerSetReadyRequest: std::cout << "headerSetReadyRequest"; break;
        case headerSetReadyResponse: std::cout << "headerSetReadyResponse"; break;
        case headerRoomPlayerJoined: std::cout << "headerRoomPlayerJoined"; break;
        case headerRoomPlayerLeft: std::cout << "headerRoomPlayerLeft"; break;
        case headerRoomStatusChanged: std::cout << "headerRoomStatusChanged"; break;
        case headerRoomPlayerReadyChanged: std::cout << "headerRoomPlayerReadyChanged"; break;
        case headerGameModeUpdate: std::cout << "headerGameModeUpdate"; break;
        case headerMatchStart: std::cout << "headerMatchStart"; break;
        case headerMatchEnd: std::cout << "headerMatchEnd"; break;
        case headerPlayerKill: std::cout << "headerPlayerKill"; break;
        case headerPlayerDeath: std::cout << "headerPlayerDeath"; break;
        case headerScoreUpdate: std::cout << "headerScoreUpdate"; break;
        case headerTowerDefenseStateUpdate: std::cout << "headerTowerDefenseStateUpdate"; break;
        case headerBuildTowerRequest: std::cout << "headerBuildTowerRequest"; break;
        case headerBuildTowerResponse: std::cout << "headerBuildTowerResponse"; break;
        case headerTowerPlaced: std::cout << "headerTowerPlaced"; break;
        case headerUpgradeTowerRequest: std::cout << "headerUpgradeTowerRequest"; break;
        case headerUpgradeTowerResponse: std::cout << "headerUpgradeTowerResponse"; break;
        case headerSellTowerRequest: std::cout << "headerSellTowerRequest"; break;
        case headerTowerSold: std::cout << "headerTowerSold"; break;
        case headerSpawnEnemy: std::cout << "headerSpawnEnemy"; break;
        case headerEnemyUpdate: std::cout << "headerEnemyUpdate"; break;
        case headerEnemyDeath: std::cout << "headerEnemyDeath"; break;
        case headerEnemyReachedBase: std::cout << "headerEnemyReachedBase"; break;
        case headerBaseHealthUpdate: std::cout << "headerBaseHealthUpdate"; break;
        case headerWaveStart: std::cout << "headerWaveStart"; break;
        case headerWaveComplete: std::cout << "headerWaveComplete"; break;
        case headerPlayerMoneyUpdate: std::cout << "headerPlayerMoneyUpdate"; break;
        case headerTowerAttack: std::cout << "headerTowerAttack"; break;
        case headerStartWaveEarly: std::cout << "headerStartWaveEarly"; break;
        case headerHordeStateUpdate: std::cout << "headerHordeStateUpdate"; break;
        case headerHordeSpawnEnemy: std::cout << "headerHordeSpawnEnemy"; break;
        case headerHordeEnemyUpdate: std::cout << "headerHordeEnemyUpdate"; break;
        case headerHordeEnemyDeath: std::cout << "headerHordeEnemyDeath"; break;
        case headerHordeWaveStart: std::cout << "headerHordeWaveStart"; break;
        case headerHordeWaveComplete: std::cout << "headerHordeWaveComplete"; break;
        case headerHordeBuyUpgrade: std::cout << "headerHordeBuyUpgrade"; break;
        case headerHordeBuyUpgradeResponse: std::cout << "headerHordeBuyUpgradeResponse"; break;
        case headerHordeBuyItem: std::cout << "headerHordeBuyItem"; break;
        case headerHordeBuyItemResponse: std::cout << "headerHordeBuyItemResponse"; break;
        case headerHordePlayerMoneyUpdate: std::cout << "headerHordePlayerMoneyUpdate"; break;
        case headerHordePlayerStatsUpdate: std::cout << "headerHordePlayerStatsUpdate"; break;
        case headerHordePlayerRespawn: std::cout << "headerHordePlayerRespawn"; break;
        case headerHordeMatchEnd: std::cout << "headerHordeMatchEnd"; break;
        case headerHordeBulletHitEnemy: std::cout << "headerHordeBulletHitEnemy"; break;
        case headerHordeEnemyAttack: std::cout << "headerHordeEnemyAttack"; break;
        case headerHordeDamageUpdate: std::cout << "headerHordeDamageUpdate"; break;
        case headerBossFightStateUpdate: std::cout << "headerBossFightStateUpdate"; break;
        case headerBossFightBossSpawn: std::cout << "headerBossFightBossSpawn"; break;
        case headerBossFightBossUpdate: std::cout << "headerBossFightBossUpdate"; break;
        case headerBossFightBossAttack: std::cout << "headerBossFightBossAttack"; break;
        case headerBossFightBossDeath: std::cout << "headerBossFightBossDeath"; break;
        case headerBossFightMinionSpawn: std::cout << "headerBossFightMinionSpawn"; break;
        case headerBossFightMinionUpdate: std::cout << "headerBossFightMinionUpdate"; break;
        case headerBossFightMinionDeath: std::cout << "headerBossFightMinionDeath"; break;
        case headerBossFightPlayerRespawn: std::cout << "headerBossFightPlayerRespawn"; break;
        case headerBossFightMatchEnd: std::cout << "headerBossFightMatchEnd"; break;
        case headerBossFightPlayerDamage: std::cout << "headerBossFightPlayerDamage"; break;
        case headerBossFightDebugRespawnBoss: std::cout << "headerBossFightDebugRespawnBoss"; break;
		case gameEndHeader: std::cout << "gameEndHeader"; break;
        default: std::cout << "UNKNOWN (" << p.header << ")"; break;
    }
    std::cout << " (" << p.header << ")" << std::endl;
    
    std::cout << "CID: " << p.cid << std::endl;
    std::cout << "Data Size: " << dataSize << " bytes" << std::endl;
    
    // Print packet data as ASCII
    if (dataSize > 0) {
        char* packetData = (char *)data + sizeof(Packet);
        std::cout << "Data (ASCII): ";
        for (size_t i = 0; i < dataSize; i++) {
            char c = packetData[i];
            if (c >= 32 && c <= 126) {  // Printable ASCII range
                std::cout << c;
            } else if (c == 0) {
                std::cout << "\\0";  // Show null terminators
            } else {
                std::cout << ".";  // Non-printable characters
            }
        }
        std::cout << std::endl;
    }
    
    std::cout << "=====================" << std::endl;
    
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

#include <gameLayer/ClientGame.h>
#include <gameLayer/packet.h>
#include <gameLayer/serverClient.h>
#include <iostream>
#include <algorithm>
#include <platform/platformInput.h>
#include <imgui.h>
#include <tuple>
#include <glui/glui.h>
#include <iomanip>

// Helper for spawn positions (copied from client.cpp)
static glm::ivec2 spawnPositions[] =
{
	{5,5},
	{2,46},
	{44,44},
	{45,4}
};

static glm::vec2 getSpawnPosition() {
	int i = rand() % 4;
	return glm::vec2(spawnPositions[i].x, spawnPositions[i].y);
}

// Helper to convert packet header to string
static const char* getPacketTypeString(int header) {
    switch(header) {
        case headerNone: return "None";
        case headerRegisterRequest: return "RegisterRequest";
        case headerRegisterResponse: return "RegisterResponse";
        case headerLoginRequest: return "LoginRequest";
        case headerLoginResponse: return "LoginResponse";
        case headerLogoutRequest: return "LogoutRequest";
        case headerLogoutResponse: return "LogoutResponse";
        case headerReceiveCIDAndData: return "ReceiveCIDAndData";
        case headerAnounceConnection: return "AnounceConnection";
        case headerUpdateConnection: return "UpdateConnection";
        case headerAnounceDisconnect: return "AnounceDisconnect";
        case headerSendBullet: return "SendBullet";
        case headerRegisterHit: return "RegisterHit";
        case headerSpawnItem: return "SpawnItem";
        case headerPickupItem: return "PickupItem";
        case headerRequestAccountInfo: return "RequestAccountInfo";
        case headerAccountInfo: return "AccountInfo";
        case headerRequestLeaderboard: return "RequestLeaderboard";
        case headerLeaderboard: return "Leaderboard";
        case headerCreateRoomRequest: return "CreateRoomRequest";
        case headerCreateRoomResponse: return "CreateRoomResponse";
        case headerJoinRoomRequest: return "JoinRoomRequest";
        case headerJoinRoomResponse: return "JoinRoomResponse";
        case headerLeaveRoomRequest: return "LeaveRoomRequest";
        case headerLeaveRoomResponse: return "LeaveRoomResponse";
        case headerGetRoomListRequest: return "GetRoomListRequest";
        case headerGetRoomListResponse: return "GetRoomListResponse";
        case headerGetRoomInfoRequest: return "GetRoomInfoRequest";
        case headerGetRoomInfoResponse: return "GetRoomInfoResponse";
        case headerStartGameRequest: return "StartGameRequest";
        case headerStartGameResponse: return "StartGameResponse";
        case headerSetReadyRequest: return "SetReadyRequest";
        case headerSetReadyResponse: return "SetReadyResponse";
        case headerRoomPlayerJoined: return "RoomPlayerJoined";
        case headerRoomPlayerLeft: return "RoomPlayerLeft";
        case headerRoomStatusChanged: return "RoomStatusChanged";
        case headerRoomPlayerReadyChanged: return "RoomPlayerReadyChanged";
        case headerGameModeUpdate: return "GameModeUpdate";
        case headerMatchStart: return "MatchStart";
        case headerMatchEnd: return "MatchEnd";
        case headerPlayerKill: return "PlayerKill";
        case headerPlayerDeath: return "PlayerDeath";
        case headerScoreUpdate: return "ScoreUpdate";
        case headerHordeStateUpdate: return "HordeStateUpdate";
        case headerHordeSpawnEnemy: return "HordeSpawnEnemy";
        case headerHordeEnemyUpdate: return "HordeEnemyUpdate";
        case headerHordeEnemyDeath: return "HordeEnemyDeath";
        case headerBossFightStateUpdate: return "BossFightStateUpdate";
        case headerBossFightBossSpawn: return "BossFightBossSpawn";
        case headerBossFightBossUpdate: return "BossFightBossUpdate";
        case headerBossFightBossAttack: return "BossFightBossAttack";
        case headerBossFightBossDeath: return "BossFightBossDeath";
        // Add more as needed
        default: return "Unknown";
    }
}

ClientGame::ClientGame() {
    // Constructor
}

ClientGame::~ClientGame() {
    // Destructor
    map.cleanup();
}


void ClientGame::reset(int mapId) {
    std::cout << "ClientGame::reset called with mapId: " << mapId << std::endl;
    
    players.clear();
    hordeEnemies.clear();
    clientMinions.clear();
    ownBullets.clear();
    otherBullets.clear();
    pickups.clear();
    chatMessages.clear();
    
    currentWave = 0;
    playerMoney = 0;
    hordeState = HordeDefense::HordeDefenseState::WAITING;
    clientBoss = BossFight::Boss();
    clientBoss.isAlive = false;
    
    isChatActive = false;
    memset(chatInputBuffer, 0, sizeof(chatInputBuffer));
   
    // camera.setDefault();

    // Map selection logic
    const char* mapFile;
    switch(mapId) {
        case 0: // Default
            mapFile = RESOURCES_PATH "mapData2.bin";
            break;
        case 1: // Industrial (Fallback)
             mapFile = RESOURCES_PATH "mapData2.bin";
             break;
        case 2: // Warehouse (Fallback)
             mapFile = RESOURCES_PATH "mapData2.bin";
             break;
        case 3: // Boss Arena
             mapFile = RESOURCES_PATH "bossFightArena.bin";
             break;
        default:
             mapFile = RESOURCES_PATH "mapData2.bin";
             break;
    }
    
    // Override if mode dictates (e.g. Boss Fight mode might imply Boss Arena?)
    // But let's respect mapId first as it matches RoomUI selection.
    
    if(!map.load(mapFile)) {
        std::cout << "Failed to load map: " << mapFile << "\n";
    } else {
        std::cout << "Loaded map: " << mapFile << " (ID: " << mapId << ")\n";
    }
}

void ClientGame::close(AccountManager& accountManager) {
    if (!network.isConnected()) { return; }
    
    // Send game end packet
    Packet p;
    p.header = gameEndHeader; 
    p.cid = network.getClientId();
    
    // We can't really send it reliably if we are closing, but let's try
    network.sendPacket(&p, sizeof(Packet), true);
    
    // Update account stats logic
    if (network.getClientId() != -1 && players.count(network.getClientId())) {
        auto& player = players[network.getClientId()];
        int32_t finalScore = player.kills; 
        
        Account* account = accountManager.getAccount(player.name);
        if (account) {
            account->totalScore += finalScore;
            accountManager.updateAccount(*account);
            std::cout << "Updated account score: " << account->totalScore << "\n";
        }
    }
    
    network.disconnect();
}

void ClientGame::update(float deltaTime, gl2d::Renderer2D& renderer, Textures& textures, const std::string& ip, const char* name, int port) {
    
    // 1. Connection management
    if (!network.isConnected()) {
        if (network.connect(ip, port, name)) {
            reset();
            std::cout << "ClientGame connected to " << ip << ":" << port << std::endl;
        } else {
            return; 
        }
    }
    
    // 2. Network processing
    processNetworkPackets();
    
    // 3. Local Game Data Updates
    // Update own bullets
    for (auto it = ownBullets.begin(); it != ownBullets.end();) {
        it->updateMove(deltaTime);
        // Simple culling
        if (glm::length(it->pos - glm::vec2(map.w/2.0f, map.h/2.0f)) > 1000.0f) { // Map size based cull? Or just large number
             it = ownBullets.erase(it);
        } else {
             ++it;
        }
    }
    // Update other bullets
     for (auto it = otherBullets.begin(); it != otherBullets.end();) {
        it->updateMove(deltaTime);
         // Cull
         ++it;
    }
    // Limit bullet count to prevent leak if culling bad
    if (ownBullets.size() > 200) ownBullets.erase(ownBullets.begin(), ownBullets.begin() + 10);
    if (otherBullets.size() > 200) otherBullets.erase(otherBullets.begin(), otherBullets.begin() + 10);
    
    
    // 4. Input & Player Control
    if (network.getClientId() != -1) {
        auto it = players.find(network.getClientId());
        if (it != players.end()) {
            phisics::Entity& player = it->second;
            
            // Only update if not paused (deltaTime > 0)
            if (deltaTime > 0 && player.health > 0) {
                // Movement is handled by server authoritative updates mostly, 
                // but client prediction or input sending happens here.
                // In original code, input was sent via packets? 
                // client.cpp:1153 had `sendPlayerData(p, false);`
                
                // We need to implement input sending!
                // Simplified for now: assume server handles position updates based on input packet?
                // No, client.cpp calculated position locally and sent it!
                // "phisics::Entity p = players[cid]; ... p.pos += ...; sendPlayerData(p, false);"
                
                // RE-IMPLEMENT MOVEMENT:
                glm::vec2 direction(0,0);
                if(platform::isKeyHeld(platform::Button::W) || platform::isKeyHeld(platform::Button::Up)) direction.y -= 1;
                if(platform::isKeyHeld(platform::Button::S) || platform::isKeyHeld(platform::Button::Down)) direction.y += 1;
                if(platform::isKeyHeld(platform::Button::A) || platform::isKeyHeld(platform::Button::Left)) direction.x -= 1;
                if(platform::isKeyHeld(platform::Button::D) || platform::isKeyHeld(platform::Button::Right)) direction.x += 1;
                
                if(glm::length(direction) > 0) {
                    direction = glm::normalize(direction);
                    player.pos += direction * player.speed * deltaTime;

                    if (map.data) {
                        // std::cout << "DEBUG: resolving constrains..." << std::endl;
                        player.resolveConstrains(map); // Collision
                        // std::cout << "DEBUG: constrains resolved." << std::endl;
                    } else {
                        // std::cout << "WARN: Map data is null, skipping collision." << std::endl;
                    }
                    
                    // Send update
                    struct NetworkPacket {
                        Packet p;
                        phisics::Entity e;
                    } data;
                    
                    data.p.header = headerUpdateConnection;
                    data.p.cid = network.getClientId();
                    
                    // Copy data
                    memcpy(&data.e, &player, sizeof(phisics::Entity));
                    
                    // std::cout << "DEBUG: sending packet..." << std::endl;
                    network.sendPacket(&data, sizeof(data), false); // Unreliable for movement
                    // std::cout << "DEBUG: packet sent." << std::endl;
                    
                    std::cout << "Action: Move - Sending Packet: " << getPacketTypeString(data.p.header) << " (Pos: " << player.pos.x << ", " << player.pos.y << ")" << std::endl;
                }
                player.lastPos = player.pos;

                updateShooting(deltaTime, player, renderer);
            }
            
            // Camera follow
            updateCamera(deltaTime, player, renderer);
        }
    }
    
    // 5. Rendering
    renderGame(renderer, textures, deltaTime);
    renderUI(renderer, textures);
}

void ClientGame::processNetworkPackets() {
    ENetEvent event;
    while (network.pollEvent(event)) {
        switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE: {
                Packet p = {};
                size_t size = {};
                auto data = parsePacket(event, p, size);
                
                std::cout << "Received Packet: " << getPacketTypeString(p.header) << " from CID " << p.cid << std::endl;
                
                if (p.header == headerAnounceConnection) {
                    if (size >= sizeof(phisics::Entity)) {
                        memcpy(&players[p.cid], data, sizeof(phisics::Entity));
                        std::cout << "DEBUG: AnounceConnection CID " << p.cid << " Name: " << players[p.cid].name << " Pos: " << players[p.cid].pos.x << "," << players[p.cid].pos.y << std::endl;
                    } else {
                         std::cerr << "CRITICAL ERROR: headerAnounceConnection packet too small! Size: " << size << " Expected: " << sizeof(phisics::Entity) << std::endl;
                    }
                }
                else if (p.header == headerReceiveCIDAndData) {
                    // Initialize local player
                    network.setClientId(p.cid);
                    
                    phisics::Entity myEntity = {};
                    glm::vec3 color = *(glm::vec3*)data;
                    myEntity.color = color;
                    myEntity.pos = getSpawnPosition();
                    myEntity.health = 100;
                    myEntity.maxHealth = 100;
                    myEntity.speed = 10.0f;
                    
                    // Set default name (will be overwritten if server sends update, or we can use local name)
                    // We don't have local name here easily without passing it down or storing it.
                    // But server broadcasts `headerAnounceConnection` to others with name.
                    // Server DOES NOT send `headerAnounceConnection` to self.
                    
                    players[p.cid] = myEntity;
                    std::cout << "Initialized local player " << p.cid << " with color " << color.x << ", " << color.y << ", " << color.z << std::endl;
                }
                else if (p.header == headerUpdateConnection) {
                    if (size >= sizeof(phisics::Entity)) {
                        memcpy(&players[p.cid], data, sizeof(phisics::Entity));
                    } else {
                         std::cerr << "CRITICAL ERROR: headerUpdateConnection packet too small! Size: " << size << " Expected: " << sizeof(phisics::Entity) << std::endl;
                    }
                }
                else if (p.header == headerAnounceDisconnect) {
                    players.erase(p.cid);
                }
                else if (p.header == headerSendBullet) {
                    otherBullets.push_back(*(phisics::Bullet *)data);
                }
                else if (p.header == headerRegisterHit) {
                    // Logic handled on server mostly for HP, but visual feedback here
                }
                else if (p.header == headerMatchStart) {
                    auto startData = *(MatchStartData*)data;
                    currentGameMode = static_cast<GameMode>(startData.gameMode);
                    reset();
                    
                    // Load map
                    const char* mapFile;
					if (startData.mapId == 3 || currentGameMode == GameMode::BOSS_FIGHT) { // 2=BOSS
						mapFile = RESOURCES_PATH "bossFightArena.bin";
					} else {
						mapFile = RESOURCES_PATH "mapData2.bin";
					}
                    std::cout << "Loading map: " << mapFile << " for mode " << (int)currentGameMode << "\n";
					if (!map.load(mapFile)) {
                         std::cout << "ERROR: Failed to load map " << mapFile << "\n";
                    }
                }
                // Horde
                else if (p.header == headerHordeStateUpdate) {
                     auto stateData = *(HordeStateUpdateData*)data;
                     hordeState = static_cast<HordeDefense::HordeDefenseState>(stateData.gameState);
                     currentWave = stateData.currentWave;
                     phaseTimer = stateData.timeRemaining;
                     enemiesAlive = stateData.enemiesRemaining;
                }
                else if (p.header == headerHordeSpawnEnemy) {
                    auto spawnData = *(HordeEnemySpawnData*)data;
                    HordeDefense::Enemy e;
                    e.id = spawnData.enemyId;
                    e.type = static_cast<HordeDefense::EnemyType>(spawnData.enemyType);
                    e.pos = {spawnData.posX, spawnData.posY};
                    e.health = spawnData.health;
                    e.maxHealth = spawnData.maxHealth;
                    hordeEnemies[e.id] = e;
                }
                 else if (p.header == headerHordeEnemyUpdate) {
					int num = event.packet->dataLength / sizeof(HordeEnemyUpdateData);
					HordeEnemyUpdateData* updates = (HordeEnemyUpdateData*)data;
					for (int i = 0; i < num; i++) {
						auto& u = updates[i];
						if(hordeEnemies.count(u.enemyId)) {
                             auto& e = hordeEnemies[u.enemyId];
                             e.pos = {u.posX, u.posY};
                             e.health = u.health;
                        }
					}
                }
                else if (p.header == headerHordeEnemyDeath) {
                    auto d = *(HordeEnemyDeathData*)data;
                    hordeEnemies.erase(d.enemyId);
                    if(d.killerCid == network.getClientId()) playerMoney += d.moneyReward;
                }
                // Boss
                else if (p.header == headerBossFightStateUpdate) {
                    auto s = *(BossFightStateUpdateData*)data;
                    clientBoss.health = s.bossHealth;
                    // ...
                }
                    auto u = *(BossFightBossUpdateData*)data;
                    clientBoss.pos = {u.posX, u.posY};
                    clientBoss.health = u.health;
                    // std::cout << "Boss update: " << u.posX << ", " << u.posY << " HP: " << u.health << "\n";
                
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                network.disconnect();
                break;
        }
    }
}

void ClientGame::updateShooting(float deltaTime, phisics::Entity& player, gl2d::Renderer2D& renderer) {
    if (platform::isLMousePressed()) { 
        phisics::Bullet b;
        b.pos = player.pos + glm::vec2(player.dimensions.x/2, player.dimensions.y/2); // Center
        b.size = 0.4f;
        b.color = {1,1,0}; // Yellow bullets
        b.cid = network.getClientId();
        
        glm::vec2 mousePos = platform::getRelMousePosition();
        glm::vec2 worldMouse = renderer.currentCamera.convertPoint(mousePos, renderer.windowW, renderer.windowH);
        
        b.direction = glm::normalize(worldMouse - b.pos) * 15.0f; 
        
        ownBullets.push_back(b);
        
        // Send bullet packet
        struct {
            Packet p;
            phisics::Bullet b;
        } dataPacket;
        
        dataPacket.p.header = headerSendBullet;
        dataPacket.p.cid = network.getClientId();
        dataPacket.b = b;
        
        network.sendPacket(&dataPacket, sizeof(dataPacket), true); // Reliable for bullets? Or false? Usually false for spam, true for spawn events.. let's use true for now to ensure it arrives.
        
        std::cout << "Action: Shoot - Sending Packet: " << getPacketTypeString(dataPacket.p.header) << std::endl;
    }
}

void ClientGame::updateCamera(float deltaTime, const phisics::Entity& player, gl2d::Renderer2D& renderer) {
    float worldMagnification = 40.0f;
    renderer.currentCamera.follow(player.pos * worldMagnification, deltaTime * 5, 3, renderer.windowW, renderer.windowH);
    
    // Clamp Camera to Map Bounds
    float mapW = map.w * worldMagnification;
    float mapH = map.h * worldMagnification;
    float viewW = renderer.windowW; // Zoom is handled by camera.zoom, but currentCamera default zoom is 1?
    // renderer.currentCamera.zoom is likely 1 unless changed.
    // The follow method might center the camera.
    
    // We need to access camera position.
    // renderer.currentCamera.position
    
    // Simple clamp if possible. currentCamera is gl2d::Camera.
    // Let's assume standard gl2d camera behavior.
    // Since we don't have easy access to camera internals' "target" without looking at gl2d, 
    // we can just rely on the fact that follow sets the position.
    
    // Only clamp if we have map data
    if(map.w > 0 && map.h > 0) {
        // This is a rough clamp, exact viewport math might depend on zoom
        if (renderer.currentCamera.position.x < 0) renderer.currentCamera.position.x = 0;
        if (renderer.currentCamera.position.y < 0) renderer.currentCamera.position.y = 0;
        // Right/Bottom bound is harder without knowing zoom effectively, 
        // but let's assume we don't want to see past mapW/mapH
        if (renderer.currentCamera.position.x + renderer.windowW > mapW) renderer.currentCamera.position.x = mapW - renderer.windowW;
        if (renderer.currentCamera.position.y + renderer.windowH > mapH) renderer.currentCamera.position.y = mapH - renderer.windowH;
        
        // Double check if map is smaller than screen
        if (mapW < renderer.windowW) renderer.currentCamera.position.x = -(renderer.windowW - mapW)/2; // Center? or 0
        if (mapH < renderer.windowH) renderer.currentCamera.position.y = -(renderer.windowH - mapH)/2;
    }
}

void ClientGame::renderGame(gl2d::Renderer2D& renderer, Textures& textures, float deltaTime) {
    float worldMagnification = 40.0f;
    
    // Render Map
    map.render(renderer, textures.sprites);
    
    // Render Enemies
    for (auto& pair : hordeEnemies) {
        auto& e = pair.second;
        glm::vec4 rect = {e.pos.x * worldMagnification, e.pos.y * worldMagnification, 0.9f*worldMagnification, 0.9f*worldMagnification};
        renderer.renderRectangle(rect, {1,0,0,1}, {0,0}, 0, textures.character);
    }
    
    // Render Boss (5x size)
    if (currentGameMode == GameMode::BOSS_FIGHT && clientBoss.isAlive) {
        // Boss size 5x
        float bossScale = 5.0f;
        glm::vec4 rect = {clientBoss.pos.x * worldMagnification, clientBoss.pos.y * worldMagnification, bossScale*worldMagnification, bossScale*worldMagnification};
        renderer.renderRectangle(rect, {0.8f, 0.1f, 0.1f, 1.0f}, {0,0}, 0, textures.character); // Reddish boss
        
        // Boss HP Bar
        if (clientBoss.maxHealth > 0) {
             float healthPct = clientBoss.health / clientBoss.maxHealth;
             glm::vec4 barRect = rect;
             barRect.y -= 20;
             barRect.w = 10; // Height
             barRect.z = rect.z; // Full width of boss
             
             renderer.renderRectangle(barRect, {0,0,0,1}, {}, 0, textures.sprites);
             barRect.z *= healthPct;
             renderer.renderRectangle(barRect, {1,0,0,1}, {}, 0, textures.sprites);
        }
    }
    
    // Render Players
    for (auto& pair : players) {
        auto& p = pair.second;
        glm::vec4 rect = {p.pos.x * worldMagnification, p.pos.y * worldMagnification, 0.9f*worldMagnification, 0.9f*worldMagnification};
        renderer.renderRectangle(rect, {1,1,1,1}, {0,0}, 0, textures.character);
        
        // Name tag
        renderer.renderText({rect.x, rect.y - 20}, p.name, textures.font, {1,1,1,1}, 0.5f);
        
        // Health bar
        if (p.maxHealth > 0) {
            float healthPct = p.health / p.maxHealth;
            glm::vec4 barRect = rect;
            barRect.y -= 10;
            barRect.w = 4; // Height (index 3)
            
            // Background (black)
            renderer.renderRectangle(barRect, {0,0,0,1}, {}, 0, textures.sprites); 
            
            // Foreground (Green/Red)
            barRect.z *= healthPct; // Width (index 2)
            glm::vec4 color = {0,1,0,1}; // Green
            if (healthPct < 0.5f) color = {1,1,0,1}; // Yellow
            if (healthPct < 0.2f) color = {1,0,0,1}; // Red
            
            renderer.renderRectangle(barRect, color, {}, 0, textures.sprites);
        }
    }
    
    // Render Bullets
    for (auto& b : ownBullets) {
        glm::vec4 rect = {b.pos.x * worldMagnification, b.pos.y * worldMagnification, 0.2f*worldMagnification, 0.2f*worldMagnification};
        renderer.renderRectangle(rect, {1,1,0,1}, {}, 0, textures.battery);
    }
     for (auto& b : otherBullets) {
        glm::vec4 rect = {b.pos.x * worldMagnification, b.pos.y * worldMagnification, 0.2f*worldMagnification, 0.2f*worldMagnification};
        renderer.renderRectangle(rect, {1,0.5,0,1}, {}, 0, textures.battery);
    }
    
}

void ClientGame::renderUI(gl2d::Renderer2D& renderer, Textures& textures) {
    // Save current camera
    auto oldCamera = renderer.currentCamera;
    
    // Reset camera for UI
    renderer.currentCamera.setDefault(); // Or position = {0,0}, zoom = 1
    renderer.currentCamera.position = {0,0};
    renderer.currentCamera.zoom = 1.0f;
    
    // Basic UI
    if(network.getClientId() != -1 && players.count(network.getClientId())) {
        auto& p = players[network.getClientId()];
        std::string hpStr = "HP: " + std::to_string((int)p.health) + "/" + std::to_string((int)p.maxHealth);
        float hpTextWidth = hpStr.length() * 10; // Approx
        renderer.renderText({(float)renderer.windowW - 150, 10}, hpStr.c_str(), textures.font, {1,0,0,1}, 1.0f);
        
        std::string moneyStr = "Money: $" + std::to_string(playerMoney);
        renderer.renderText({(float)renderer.windowW - 150, 50}, moneyStr.c_str(), textures.font, {1,1,0,1}, 1.0f);
        
        if (currentGameMode == GameMode::HORDE_DEFENSE) { // Horde
             std::string waveStr = "Wave: " + std::to_string(currentWave);
             renderer.renderText({(float)renderer.windowW/2 - 50, 10}, waveStr.c_str(), textures.font, {1,1,1,1}, 1.0f);
        }
    }
    
    // Restore camera
    renderer.currentCamera = oldCamera;
}

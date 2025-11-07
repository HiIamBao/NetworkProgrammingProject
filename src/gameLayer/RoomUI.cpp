#include "gameLayer/RoomUI.h"
#include <cstring>
#include <algorithm>
#include <iostream>

RoomUI::RoomUI() 
    : currentState(RoomUIState::NONE)
    , isLoggedIn(false)
    , selectedRoomIndex(-1)
    , roomListRefreshTimer(0.0f)
    , selectedMaxPlayers(1)
    , selectedGameMode(0)
    , selectedMapId(0)
    , joiningRoomId(-1)
    , currentRoomId(-1)
    , inRoom(false)
    , isHost(false)
    , isReady(false)
    , statusColor(RoomUIColors::White)
    , messageTimer(0.0f)
    , networkContext(nullptr)
{
    memset(roomNameInput, 0, sizeof(roomNameInput));
    memset(roomPasswordInput, 0, sizeof(roomPasswordInput));
    memset(joinPasswordInput, 0, sizeof(joinPasswordInput));
}

RoomUI::~RoomUI() {
}

void RoomUI::render(gl2d::Renderer2D& renderer, gl2d::Font& font, float deltaTime) {
    // Update timers
    if (messageTimer > 0.0f) {
        messageTimer -= deltaTime;
    }
    
    roomListRefreshTimer += deltaTime;
    
    // Auto-refresh room list every 5 seconds when browsing
    if (currentState == RoomUIState::ROOM_BROWSER && roomListRefreshTimer >= 5.0f) {
        roomListRefreshTimer = 0.0f;
        if (onRequestRoomList) {
            onRequestRoomList();
        }
    }
    
    // Render based on state
    switch (currentState) {
        case RoomUIState::ROOM_BROWSER:
            renderRoomBrowser(renderer, font);
            break;
        case RoomUIState::CREATE_ROOM:
            renderCreateRoom(renderer, font);
            break;
        case RoomUIState::ROOM_LOBBY:
            renderRoomLobby(renderer, font);
            break;
        case RoomUIState::JOINING_ROOM:
            renderJoiningRoom(renderer, font);
            break;
        default:
            break;
    }
}

void RoomUI::setState(RoomUIState newState) {
    currentState = newState;
    
    if (newState == RoomUIState::ROOM_BROWSER) {
        // Request room list when entering browser
        roomListRefreshTimer = 0.0f;
        if (onRequestRoomList) {
            onRequestRoomList();
        }
    } else if (newState == RoomUIState::CREATE_ROOM) {
        clearCreateRoomInputs();
    }
}

void RoomUI::renderRoomBrowser(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("===== ROOM BROWSER =====", RoomUIColors::Primary);
    glui::Space(20);
    
    // Status message
    if (messageTimer > 0.0f) {
        glui::Text(statusMessage.c_str(), statusColor);
        glui::Space(10);
    }
    
    // Buttons
    if (glui::Button("Create Room", RoomUIColors::Success)) {
        setState(RoomUIState::CREATE_ROOM);
    }
    
    if (glui::Button("Refresh List", RoomUIColors::Primary)) {
        roomListRefreshTimer = 0.0f;
        if (onRequestRoomList) {
            onRequestRoomList();
        }
        showMessage("Refreshing...", RoomUIColors::White);
    }
    
    if (glui::Button("Back to Menu", RoomUIColors::Gray)) {
        setState(RoomUIState::NONE);
    }
    
    
    glui::Space(20);
    glui::Text("Available Rooms:", RoomUIColors::White);
    glui::Space(10);
    
    // Room list
    if (roomList.empty()) {
        glui::Text("No rooms available.", RoomUIColors::Gray);
        glui::Space(10);
        glui::Text("Create your own room to host a game,", RoomUIColors::White);
        glui::Text("or use 'Join Game' from main menu", RoomUIColors::White);
        glui::Text("to connect via IP address.", RoomUIColors::White);
    } else {
        for (size_t i = 0; i < roomList.size(); i++) {
            const auto& room = roomList[i];
            
            glui::PushId(i);
            
            // Room name
            char roomTitle[128];
            if (room.hasPassword) {
                snprintf(roomTitle, sizeof(roomTitle), "[LOCKED] %s", room.roomName);
            } else {
                snprintf(roomTitle, sizeof(roomTitle), "%s", room.roomName);
            }
            glui::Text(roomTitle, RoomUIColors::White);
            
            // Room info
            char info[256];
            snprintf(info, sizeof(info), "  Players: %d/%d | Mode: %s | Status: %s", 
                     room.currentPlayers, room.maxPlayers,
                     getGameModeName(room.gameMode), 
                     getRoomStatusName(room.status));
            glui::Text(info, RoomUIColors::Gray);
            
            // Host info
            char hostInfo[128];
            snprintf(hostInfo, sizeof(hostInfo), "  Host: %s", room.hostUsername);
            glui::Text(hostInfo, RoomUIColors::Host);
            
            // Join button
            bool canJoin = (room.status == 0 && room.currentPlayers < room.maxPlayers);
            
            if (canJoin) {
                char joinButton[64];
                snprintf(joinButton, sizeof(joinButton), "Join Room##%zu", i);
                if (glui::Button(joinButton, RoomUIColors::Success)) {
                    selectedRoomIndex = i;
                    joiningRoomId = room.roomId;
                    
                    if (room.hasPassword) {
                        memset(joinPasswordInput, 0, sizeof(joinPasswordInput));
                    } else {
                        if (onJoinRoom) {
                            JoinRoomData joinData;
                            joinData.roomId = room.roomId;
                            memset(joinData.password, 0, sizeof(joinData.password));
                            onJoinRoom(joinData);
                            setState(RoomUIState::JOINING_ROOM);
                        }
                    }
                }
            } else {
                glui::Button("Full/In Game", RoomUIColors::Gray);
            }
            
            glui::Space(15);
            glui::PopId();
        }
    }
    
    // Password input for selected locked room
    if (selectedRoomIndex >= 0 && selectedRoomIndex < (int)roomList.size()) {
        const auto& selectedRoom = roomList[selectedRoomIndex];
        if (selectedRoom.hasPassword && joiningRoomId == selectedRoom.roomId) {
            glui::Space(20);
            glui::Text("===== ENTER PASSWORD =====", RoomUIColors::Warning);
            glui::Space(10);
            
            glui::InputText("Password:", joinPasswordInput, sizeof(joinPasswordInput), RoomUIColors::Panel);
            
            if (glui::Button("Join with Password", RoomUIColors::Success)) {
                if (onJoinRoom) {
                    JoinRoomData joinData;
                    joinData.roomId = selectedRoom.roomId;
                    strncpy(joinData.password, joinPasswordInput, sizeof(joinData.password) - 1);
                    onJoinRoom(joinData);
                    setState(RoomUIState::JOINING_ROOM);
                }
            }
            
            if (glui::Button("Cancel", RoomUIColors::Gray)) {
                joiningRoomId = -1;
                clearJoinInputs();
            }
        }
    }
}

void RoomUI::renderCreateRoom(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("===== CREATE NEW ROOM =====", RoomUIColors::Primary);
    glui::Space(20);
    
    if (messageTimer > 0.0f) {
        glui::Text(statusMessage.c_str(), statusColor);
        glui::Space(10);
    }
    
    glui::Text("Room Name:", RoomUIColors::White);
    glui::InputText("##roomname", roomNameInput, sizeof(roomNameInput), RoomUIColors::Panel);
    glui::Space(10);
    
    glui::Text("Password (optional):", RoomUIColors::White);
    glui::InputText("##roompass", roomPasswordInput, sizeof(roomPasswordInput), RoomUIColors::Panel);
    glui::Space(10);
    
    glui::Text("Max Players:", RoomUIColors::White);
    const char* playerOptions[] = {"2", "4", "6", "8"};
    
    // Render buttons with visual selection indicator
    for (int i = 0; i < 4; i++) {
        char buttonLabel[64];
        // Add visual indicator to show selection
        if (selectedMaxPlayers == i) {
            snprintf(buttonLabel, sizeof(buttonLabel), ">>> %s Players <<<##maxp%d", playerOptions[i], i);
        } else {
            snprintf(buttonLabel, sizeof(buttonLabel), "    %s Players    ##maxp%d", playerOptions[i], i);
        }
        
        // Use bright colors to make selection obvious
        glm::vec4 btnColor = (selectedMaxPlayers == i) ? RoomUIColors::Success : RoomUIColors::Gray;
        if (glui::Button(buttonLabel, btnColor)) {
            selectedMaxPlayers = i;
        }
    }
    glui::Space(10);
    
    glui::Text("Game Mode:", RoomUIColors::White);
    const char* modeOptions[] = {"Cooperative", "Team Deathmatch", "Free-for-All", "Custom"};
    
    // Render buttons with visual selection indicator
    for (int i = 0; i < 4; i++) {
        char buttonLabel[64];
        // Add visual indicator to show selection
        if (selectedGameMode == i) {
            snprintf(buttonLabel, sizeof(buttonLabel), ">>> %s <<<##mode%d", modeOptions[i], i);
        } else {
            snprintf(buttonLabel, sizeof(buttonLabel), "    %s    ##mode%d", modeOptions[i], i);
        }
        
        glm::vec4 btnColor = (selectedGameMode == i) ? RoomUIColors::Success : RoomUIColors::Gray;
        if (glui::Button(buttonLabel, btnColor)) {
            selectedGameMode = i;
        }
    }
    glui::Space(10);
    
    glui::Text("Map:", RoomUIColors::White);
    const char* mapOptions[] = {"Default Map", "Industrial", "Warehouse"};
    
    // Render buttons with visual selection indicator  
    for (int i = 0; i < 3; i++) {
        char buttonLabel[64];
        // Add visual indicator to show selection
        if (selectedMapId == i) {
            snprintf(buttonLabel, sizeof(buttonLabel), ">>> %s <<<##map%d", mapOptions[i], i);
        } else {
            snprintf(buttonLabel, sizeof(buttonLabel), "    %s    ##map%d", mapOptions[i], i);
        }
        
        glm::vec4 btnColor = (selectedMapId == i) ? RoomUIColors::Success : RoomUIColors::Gray;
        if (glui::Button(buttonLabel, btnColor)) {
            selectedMapId = i;
        }
    }
    
    glui::Space(20);
    
    if (glui::Button("Create Room", RoomUIColors::Success)) {
        if (strlen(roomNameInput) == 0) {
            showMessage("Please enter a room name!", RoomUIColors::Error);
        } else if (onCreateRoom) {
            CreateRoomData createData;
            strncpy(createData.roomName, roomNameInput, sizeof(createData.roomName) - 1);
            
            bool hasPassword = strlen(roomPasswordInput) > 0;
            if (hasPassword) {
                strncpy(createData.password, roomPasswordInput, sizeof(createData.password) - 1);
            } else {
                memset(createData.password, 0, sizeof(createData.password));
            }
            
            int maxPlayersValues[] = {2, 4, 6, 8};
            createData.maxPlayers = maxPlayersValues[selectedMaxPlayers];
            createData.gameMode = selectedGameMode;
            createData.mapId = selectedMapId;
            
            onCreateRoom(createData);
            showMessage("Creating room...", RoomUIColors::White);
        }
    }
    
    if (glui::Button("Cancel", RoomUIColors::Gray)) {
        setState(RoomUIState::ROOM_BROWSER);
    }
}

void RoomUI::renderRoomLobby(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    char title[128];
    if (isHost) {
        snprintf(title, sizeof(title), "===== %s [HOST] =====", currentRoomInfo.roomName);
    } else {
        snprintf(title, sizeof(title), "===== %s =====", currentRoomInfo.roomName);
    }
    glui::Text(title, RoomUIColors::Primary);
    glui::Space(10);
    
    char info[256];
    snprintf(info, sizeof(info), "Mode: %s | Map: %d | Status: %s",
             getGameModeName(currentRoomInfo.gameMode),
             currentRoomInfo.mapId,
             getRoomStatusName(currentRoomInfo.status));
    glui::Text(info, RoomUIColors::White);
    glui::Space(10);
    
    if (messageTimer > 0.0f) {
        glui::Text(statusMessage.c_str(), statusColor);
        glui::Space(10);
    }
    
    char playerHeader[64];
    snprintf(playerHeader, sizeof(playerHeader), "Players (%zu/%d):",
             roomPlayers.size(), currentRoomInfo.maxPlayers);
    glui::Text(playerHeader, RoomUIColors::White);
    glui::Space(10);
    
    for (const auto& player : roomPlayers) {
        glm::vec4 playerColor = player.isReady ? RoomUIColors::Success : RoomUIColors::Gray;
        
        char playerText[128];
        bool playerIsHost = (std::string(player.username) == std::string(currentRoomInfo.hostUsername));
        
        if (playerIsHost) {
            snprintf(playerText, sizeof(playerText), "  [HOST] %s - %s",
                     player.username, player.isReady ? "READY" : "Not Ready");
            glui::Text(playerText, RoomUIColors::Host);
        } else {
            snprintf(playerText, sizeof(playerText), "  %s - %s",
                     player.username, player.isReady ? "READY" : "Not Ready");
            glui::Text(playerText, playerColor);
        }
    }
    
    glui::Space(20);
    
    if (!isHost) {
        glm::vec4 readyColor = isReady ? RoomUIColors::Warning : RoomUIColors::Success;
        const char* readyText = isReady ? "Unready" : "Ready";
        
        if (glui::Button(readyText, readyColor)) {
            if (onSetReady) {
                isReady = !isReady;
                onSetReady(isReady);
            }
        }
    } else {
        bool allReady = true;
        bool hasEnoughPlayers = roomPlayers.size() >= 2;
        
        for (const auto& player : roomPlayers) {
            bool playerIsHost = (std::string(player.username) == std::string(currentRoomInfo.hostUsername));
            if (!playerIsHost && !player.isReady) {
                allReady = false;
                break;
            }
        }
        
        bool canStart = allReady && hasEnoughPlayers && currentRoomInfo.status == 0;
        glm::vec4 startColor = canStart ? RoomUIColors::Success : RoomUIColors::Gray;
        
        if (glui::Button("Start Game", startColor)) {
            if (canStart && onStartGame) {
                onStartGame();
            } else if (!hasEnoughPlayers) {
                showMessage("Need at least 2 players!", RoomUIColors::Error);
            } else if (!allReady) {
                showMessage("All players must be ready!", RoomUIColors::Error);
            }
        }
    }
    
    if (glui::Button("Leave Room", RoomUIColors::Error)) {
        if (onLeaveRoom) {
            onLeaveRoom();
        }
    }
}

void RoomUI::renderJoiningRoom(gl2d::Renderer2D& renderer, gl2d::Font& font) {
    glui::Text("===== JOINING ROOM =====", RoomUIColors::Primary);
    glui::Space(20);
    glui::Text("Please wait...", RoomUIColors::White);
    glui::Space(20);
    
    if (glui::Button("Cancel", RoomUIColors::Gray)) {
        setState(RoomUIState::ROOM_BROWSER);
    }
}

void RoomUI::handleCreateRoomResponse(const CreateRoomResponse& response) {
    if (response.success) {
        currentRoomId = response.roomId;
        inRoom = true;
        isHost = true;
        isReady = true;
        
        if (onRequestRoomInfo) {
            onRequestRoomInfo(response.roomId);
        }
        
        setState(RoomUIState::ROOM_LOBBY);
        showMessage("Room created successfully!", RoomUIColors::Success);
    } else {
        showMessage(response.message, RoomUIColors::Error);
        setState(RoomUIState::ROOM_BROWSER);
    }
}

void RoomUI::handleJoinRoomResponse(const JoinRoomResponse& response) {
    if (response.success) {
        currentRoomId = response.roomId;
        inRoom = true;
        isHost = false;
        isReady = false;
        
        if (onRequestRoomInfo) {
            onRequestRoomInfo(response.roomId);
        }
        
        setState(RoomUIState::ROOM_LOBBY);
        showMessage("Joined room successfully!", RoomUIColors::Success);
    } else {
        showMessage(response.message, RoomUIColors::Error);
        setState(RoomUIState::ROOM_BROWSER);
    }
}

void RoomUI::handleRoomListResponse(const std::vector<RoomInfoData>& rooms) {
    roomList = rooms;
    selectedRoomIndex = -1;
}

void RoomUI::handleRoomInfoResponse(const RoomInfoData& info, const std::vector<PlayerInRoomData>& players) {
    currentRoomInfo = info;
    roomPlayers = players;
    
    // Update our host status by checking if we're the host
    isHost = (currentUsername == std::string(info.hostUsername));
    if (isHost) {
        isReady = true;  // Host is always ready
    }
}

void RoomUI::handlePlayerJoined(const PlayerJoinedData& data) {
    bool found = false;
    for (auto& player : roomPlayers) {
        if (strcmp(player.username, data.username) == 0) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        PlayerInRoomData newPlayer;
        strncpy(newPlayer.username, data.username, sizeof(newPlayer.username) - 1);
        newPlayer.isReady = false;
        newPlayer.team = -1;
        roomPlayers.push_back(newPlayer);
        
        std::string msg = std::string(data.username) + " joined the room";
        showMessage(msg, RoomUIColors::Success);
    }
}

void RoomUI::handlePlayerLeft(const PlayerLeftData& data) {
    auto it = std::remove_if(roomPlayers.begin(), roomPlayers.end(),
        [&data](const PlayerInRoomData& player) {
            return strcmp(player.username, data.username) == 0;
        });
    
    if (it != roomPlayers.end()) {
        roomPlayers.erase(it, roomPlayers.end());
        
        std::string msg = std::string(data.username) + " left the room";
        showMessage(msg, RoomUIColors::Warning);
    }
}

void RoomUI::handlePlayerReadyChanged(const PlayerReadyChangedData& data) {
    for (auto& player : roomPlayers) {
        if (strcmp(player.username, data.username) == 0) {
            player.isReady = data.isReady;
            
            std::string msg = std::string(data.username) + (data.isReady ? " is ready" : " is not ready");
            showMessage(msg, RoomUIColors::White);
            break;
        }
    }
}

void RoomUI::handleRoomStatusChanged(const RoomStatusChangedData& data) {
    currentRoomInfo.status = data.newStatus;
    
    if (data.newStatus == 1) {
        showMessage("Game starting...", RoomUIColors::Success);
    } else if (data.newStatus == 2) {
        showMessage("Game finished!", RoomUIColors::Success);
    }
}

void RoomUI::handleLeaveRoomResponse() {
    inRoom = false;
    isHost = false;
    isReady = false;
    currentRoomId = -1;
    roomPlayers.clear();
    
    setState(RoomUIState::ROOM_BROWSER);
    showMessage("Left room", RoomUIColors::White);
}

void RoomUI::showMessage(const std::string& message, const glm::vec4& color) {
    statusMessage = message;
    statusColor = color;
    messageTimer = 3.0f;
}

void RoomUI::clearCreateRoomInputs() {
    memset(roomNameInput, 0, sizeof(roomNameInput));
    memset(roomPasswordInput, 0, sizeof(roomPasswordInput));
    selectedMaxPlayers = 1;
    selectedGameMode = 0;
    selectedMapId = 0;
}

void RoomUI::clearJoinInputs() {
    memset(joinPasswordInput, 0, sizeof(joinPasswordInput));
    joiningRoomId = -1;
}

const char* RoomUI::getGameModeName(int mode) {
    switch (mode) {
        case 0: return "Classic";
        case 1: return "Team";
        case 2: return "FFA";
        case 3: return "Custom";
        default: return "Unknown";
    }
}

const char* RoomUI::getRoomStatusName(int status) {
    switch (status) {
        case 0: return "Waiting";
        case 1: return "In Game";
        case 2: return "Finished";
        default: return "Unknown";
    }
}

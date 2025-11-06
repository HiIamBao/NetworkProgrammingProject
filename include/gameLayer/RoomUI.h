#pragma once

#include <string>
#include <vector>
#include <functional>
#include "gl2d/gl2d.h"
#include "glui/glui.h"
#include "packet.h"

enum class RoomUIState {
    NONE,              // Not in room system
    ROOM_BROWSER,      // Browsing available rooms
    CREATE_ROOM,       // Creating a new room
    ROOM_LOBBY,        // Inside a room
    JOINING_ROOM       // Waiting for join response
};

namespace RoomUIColors {
    inline const glm::vec4 Primary = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);
    inline const glm::vec4 Success = glm::vec4(0.2f, 0.8f, 0.3f, 1.0f);
    inline const glm::vec4 Error = glm::vec4(0.9f, 0.2f, 0.2f, 1.0f);
    inline const glm::vec4 Warning = glm::vec4(1.0f, 0.7f, 0.0f, 1.0f);
    inline const glm::vec4 Host = glm::vec4(1.0f, 0.84f, 0.0f, 1.0f);  // Gold
    inline const glm::vec4 Panel = glm::vec4(0.15f, 0.15f, 0.2f, 0.95f);
    inline const glm::vec4 White = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    inline const glm::vec4 Gray = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
}

class RoomUI {
private:
    RoomUIState currentState;
    std::string currentUsername;
    bool isLoggedIn;
    
    // Room list
    std::vector<RoomInfoData> roomList;
    int selectedRoomIndex;
    float roomListRefreshTimer;
    
    // Create room inputs
    char roomNameInput[32];
    char roomPasswordInput[32];
    int selectedMaxPlayers;
    int selectedGameMode;
    int selectedMapId;
    
    // Join room
    char joinPasswordInput[32];
    int joiningRoomId;
    
    // Current room data
    int currentRoomId;
    bool inRoom;
    bool isHost;
    bool isReady;
    std::vector<PlayerInRoomData> roomPlayers;
    RoomInfoData currentRoomInfo;
    
    // Message display
    std::string statusMessage;
    glm::vec4 statusColor;
    float messageTimer;
    
    // Callback for sending packets
    void* networkContext;  // Can store ENetPeer or similar
    
public:
    RoomUI();
    ~RoomUI();
    
    // Main render function
    void render(gl2d::Renderer2D& renderer, gl2d::Font& font, float deltaTime);
    
    // State management
    void setState(RoomUIState newState);
    RoomUIState getState() const { return currentState; }
    void setUsername(const std::string& username) { currentUsername = username; isLoggedIn = !username.empty(); }
    void setNetworkContext(void* context) { networkContext = context; }
    
    // Room state
    bool isInRoom() const { return inRoom; }
    int getCurrentRoomId() const { return currentRoomId; }
    
    // Packet response handlers
    void handleCreateRoomResponse(const CreateRoomResponse& response);
    void handleJoinRoomResponse(const JoinRoomResponse& response);
    void handleRoomListResponse(const std::vector<RoomInfoData>& rooms);
    void handleRoomInfoResponse(const RoomInfoData& info, const std::vector<PlayerInRoomData>& players);
    void handlePlayerJoined(const PlayerJoinedData& data);
    void handlePlayerLeft(const PlayerLeftData& data);
    void handlePlayerReadyChanged(const PlayerReadyChangedData& data);
    void handleRoomStatusChanged(const RoomStatusChangedData& data);
    void handleLeaveRoomResponse();
    
    // Request packet sending (callbacks that need to be connected)
    std::function<void()> onRequestRoomList;
    std::function<void(const CreateRoomData&)> onCreateRoom;
    std::function<void(const JoinRoomData&)> onJoinRoom;
    std::function<void()> onLeaveRoom;
    std::function<void(bool)> onSetReady;
    std::function<void()> onStartGame;
    std::function<void(int)> onRequestRoomInfo;
    
private:
    // Screen rendering
    void renderRoomBrowser(gl2d::Renderer2D& renderer, gl2d::Font& font);
    void renderCreateRoom(gl2d::Renderer2D& renderer, gl2d::Font& font);
    void renderRoomLobby(gl2d::Renderer2D& renderer, gl2d::Font& font);
    void renderJoiningRoom(gl2d::Renderer2D& renderer, gl2d::Font& font);
    
    // Utilities
    void showMessage(const std::string& message, const glm::vec4& color);
    void clearCreateRoomInputs();
    void clearJoinInputs();
    const char* getGameModeName(int mode);
    const char* getRoomStatusName(int status);
};

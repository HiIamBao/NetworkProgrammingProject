#include "gameLayer.h"
#include "gl2d/gl2d.h"
#include "platformInput.h"
#include "imgui.h"
#include <iostream>
#include <sstream>
#include "Phisics.h"
#include <enet/enet.h>
#include "glui/glui.h"
#include "serverClient.h"
#include "AccountManager.h"
#include "SessionManager.h"
#include "AccountUI.h"
#include "RoomUI.h"
#include "RoomHandler.h"
#include "RoomManager.h"
#include "LANDiscovery.h"
#include "MultiRoomManager.h"
#include <thread>
#include <ctime>
#include <GLFW/glfw3.h>  // For GLFW_KEY_ESCAPE
#include <cstring>
#include <packet.h>
gl2d::Renderer2D renderer;

Textures textures;
// Global account management
static AccountManager* g_accountManager = nullptr;
static SessionManager* g_sessionManager = nullptr;
static AccountUI* g_accountUI = nullptr;

// Global room management
static RoomManager* g_roomManager = nullptr;
static RoomHandler* g_roomHandler = nullptr;
static RoomUI* g_roomUI = nullptr;

// Global LAN discovery
static LANDiscovery* g_lanDiscovery = nullptr;

// Global multi-room manager
static MultiRoomManager* g_multiRoomManager = nullptr;

bool initGame()
{
	renderer.create();
	textures.font.createFromFile(RESOURCES_PATH "font/ANDYB.TTF");
	textures.sprites.loadFromFileWithPixelPadding(RESOURCES_PATH "jawbreaker_tiles.png", tiles::pixelSize, true, true);
	textures.character.loadFromFile(RESOURCES_PATH "character2.png", true, true);
	textures.medKit.loadFromFile(RESOURCES_PATH "medkit.png", true, true);
	textures.battery.loadFromFile(RESOURCES_PATH "battery.png", true, true);
	textures.cross.loadFromFile(RESOURCES_PATH "cross.png", true, true);
	textures.accountBackground.loadFromFile(RESOURCES_PATH "husthehebg.jpg");
	glui::gluiInit();

	if (enet_initialize() != 0)
	{
		return false;
	}

	std::srand(std::time(0));
	
	// Initialize account management system
	g_accountManager = new AccountManager();
	if (!g_accountManager->initialize(RESOURCES_PATH "game_accounts.db")) {
		std::cerr << "Warning: Failed to initialize account manager" << std::endl;
		// Continue anyway - game can run without accounts
	}
	
	g_sessionManager = new SessionManager(g_accountManager);
	g_accountUI = new AccountUI(g_accountManager, g_sessionManager);

	// Initialize room management system
	g_roomManager = new RoomManager();
	g_roomHandler = new RoomHandler(g_accountManager, g_sessionManager, g_roomManager);
	g_roomUI = new RoomUI();
	
	// Initialize LAN discovery
	g_lanDiscovery = new LANDiscovery();
	
	// Initialize multi-room manager
	g_multiRoomManager = new MultiRoomManager();

	return true;
}


bool gameLogic(float deltaTime)
{
#pragma region init stuff
	int w = 0; int h = 0;
	w= platform::getWindowSizeX();
	h = platform::getWindowSizeY();
	
	renderer.updateWindowMetrics(w, h);
	renderer.clearScreen();
#pragma endregion


	//0 main menu / account system
	//1 client
	//2 server
	static int state = 0;
	static char ip[17] = {};
	static char name[playerNameSize] = {};
	static int currentPort = 7778;  // Track which port to connect to
	static bool isPaused = false;  // Track if game is paused
	static bool roomUIInitialized = false;  // Track if room UI callbacks are set up (at file scope for reset access)
	static bool wasJoined = false;  // Track if client was joined (for disconnect detection)
	
	if (state == 0)
	{
		// Use account UI system
		if (g_accountUI) {
			UIState uiState = g_accountUI->getState();
			
			// Render account UI
			g_accountUI->render(renderer, textures.font, textures, deltaTime);
			
			// Handle room browser state
			if (uiState == UIState::BROWSE_ROOMS && g_accountUI->getIsLoggedIn()) {
				// Set player name and username for room system
				strncpy(name, g_accountUI->getCurrentUsername().c_str(), playerNameSize - 1);
				name[playerNameSize - 1] = '\0';
				
				// Initialize room UI with current username and setup callbacks (ONCE)
				if (g_roomUI && !roomUIInitialized) {
					g_roomUI->setUsername(g_accountUI->getCurrentUsername());
					
					// Clear any stale server data and start fresh
					if (g_lanDiscovery) {
						g_lanDiscovery->clearDiscoveredServers();
						if (!g_lanDiscovery->isListening()) {
							g_lanDiscovery->startListening();
						}
					}
					
					// Setup simplified callbacks for local hosting with LAN discovery
					// When user clicks "Create Room", start a server and broadcast it
					g_roomUI->onCreateRoom = [&name, &state, &ip, &currentPort](const CreateRoomData& data) {
						// Use MultiRoomManager to create room
						if (g_multiRoomManager && g_accountUI) {
							int roomSlot = g_multiRoomManager->createRoom(
								data.roomName,
								g_accountUI->getCurrentUsername(),
								data.maxPlayers,
								data.gameMode,
								data.mapId
							);						
						if (roomSlot >= 0) {
							std::cout << "Room created successfully in slot " << roomSlot << std::endl;
							
							// Wait a moment for server to start
							std::this_thread::sleep_for(std::chrono::milliseconds(300));
							
							// Get room info
							RoomInfo room = g_multiRoomManager->getRoomInfo(roomSlot);
							
							// Verify server actually started on the assigned port
							if (!room.active || !isServerRunning(room.port)) {
								std::cout << "ERROR: Server failed to start on port " << room.port << std::endl;
								g_multiRoomManager->stopRoom(roomSlot);
								return;
							}
							
							if (room.active) {
								// Update LAN discovery to broadcast this room
								if (g_lanDiscovery) {
									// Stop old broadcast
									if (g_lanDiscovery->isBroadcasting()) {
										g_lanDiscovery->stopBroadcasting();
									}
									
									// Start broadcasting the room with correct port, game mode, and map
									g_lanDiscovery->startBroadcasting(
										room.roomName,
										room.hostName,
										room.port,
										room.gameMode,
										room.mapId
									);
									g_lanDiscovery->updateServerInfo(room.currentPlayers, room.maxPlayers);
								}
								
								// Connect client to the room's server with the correct port
								resetClient();
								
								// Set IP and port for connection
								strcpy(ip, "127.0.0.1");
								currentPort = room.port;  // Store the port for client connection
								state = 2; // Switch to hosting state
								
								std::cout << "Host connecting to own server on port " << room.port << std::endl;
							}
					} else {
						std::cout << "Failed to create room! Maximum " << g_multiRoomManager->getActiveRoomCount() 
								  << " rooms already active." << std::endl;
						}
					}
				};
				
				// When user clicks "Join Room", connect to discovered server
					g_roomUI->onJoinRoom = [&state, &ip, &currentPort](const JoinRoomData& data) {
						// Find the server by room ID in discovered servers
						if (g_lanDiscovery) {
							auto servers = g_lanDiscovery->getDiscoveredServers();
							if (data.roomId > 0 && data.roomId <= (int)servers.size()) {
								const auto& server = servers[data.roomId - 1];
								
								// Set IP and port to the discovered server's address
								strncpy(ip, server.ipAddress.c_str(), 16);
								ip[16] = '\0';
								currentPort = server.port;  // Store the port for client connection
								
								resetClient();
								state = 1; // Switch to client state
								
								std::cout << "Connecting to server at " << ip << ":" << server.port << std::endl;
							}
						}
					};
					
					// Start game callback - send start request to server for boss fight mode
					g_roomUI->onStartGame = []() {
						// Access server and cid from client.cpp (they're global variables)
						extern ENetPeer *server;
						extern int32_t cid;
						extern bool joined;
						
						if (!joined || !server) {
							std::cout << "Cannot start game: Not connected to server" << std::endl;
							return;
						}
						
						// Check if we're in boss fight mode
						if (g_roomUI) {
							RoomInfoData roomInfo = g_roomUI->getCurrentRoomInfo();
							if (roomInfo.gameMode == 5) {  // BOSS_FIGHT = 5
								// Send boss fight start request packet
								Packet p;
								p.header = headerBossFightStartRequest;
								p.cid = cid;
								sendPacket(server, p, nullptr, 0, true, 0);
								std::cout << "Sent boss fight start request to server (CID: " << cid << ")" << std::endl;
							} else {
								std::cout << "Start game requested but not in boss fight mode (mode: " << roomInfo.gameMode << ")" << std::endl;
							}
						}
					};
					
					// Leave room = stop broadcasting and go back to menu
					g_roomUI->onLeaveRoom = []() {
						// Stop broadcasting if we were hosting
						if (g_lanDiscovery && g_lanDiscovery->isBroadcasting()) {
							g_lanDiscovery->stopBroadcasting();
						}
						
						if (isServerRunning()) {
							closeServer();
							std::this_thread::sleep_for(std::chrono::milliseconds(200));
						}
						
						resetClient();
						
						if (g_accountUI) {
							g_accountUI->setState(UIState::MAIN_MENU);
						}
					};
					
					// Room list request - get discovered LAN servers
					g_roomUI->onRequestRoomList = []() {
						if (g_roomUI && g_lanDiscovery) {
							auto servers = g_lanDiscovery->getDiscoveredServers();
							std::vector<RoomInfoData> rooms;
							
							int roomId = 1;
							for (const auto& server : servers) {
								RoomInfoData room;
								room.roomId = roomId++;
								strncpy(room.roomName, server.serverName.c_str(), sizeof(room.roomName) - 1);
								strncpy(room.hostUsername, server.hostName.c_str(), sizeof(room.hostUsername) - 1);
								room.currentPlayers = server.playerCount;
								room.maxPlayers = server.maxPlayers;
								room.gameMode = server.gameMode;  // Use actual game mode from server
								room.mapId = server.mapId;        // Use actual map from server
								room.status = 0; // WAITING
								room.hasPassword = false;
								rooms.push_back(room);
							}
							
							g_roomUI->handleRoomListResponse(rooms);
							std::cout << "Found " << rooms.size() << " LAN server(s)" << std::endl;
						}
					};
					
					g_roomUI->setState(RoomUIState::ROOM_BROWSER);
					roomUIInitialized = true;
				}
				
				// Render room UI every frame
				if (g_roomUI) {
					// Render room UI (this overrides the account UI temporarily)
					g_roomUI->render(renderer, textures.font, deltaTime);
					
					// Check if user exited room browser
					if (g_roomUI->getState() == RoomUIState::NONE) {
						// Stop listening for LAN servers when exiting room browser
						if (g_lanDiscovery && g_lanDiscovery->isListening()) {
							g_lanDiscovery->stopListening();
						}
						g_accountUI->setState(UIState::MAIN_MENU);
						roomUIInitialized = false;  // Reset for next time
					}
				}
			}
			// Handle transitions to game modes
			else if (uiState == UIState::HOST_SERVER && g_accountUI->getIsLoggedIn()) {
				// Set player name from logged in account
				strncpy(name, g_accountUI->getCurrentUsername().c_str(), playerNameSize - 1);
				name[playerNameSize - 1] = '\0';
				
				// Show server setup UI
				glui::Space(20);
				glui::Text("=== Host Game Server ===", Colors_White);
				
				// Check server status
				if (isServerRunning()) {
					glui::Text("Server is already running!", glm::vec4(1.0f, 0.5f, 0.2f, 1.0f));
					glui::Text("Join as host or stop the server first.", Colors_White);
				} else {
					glui::Text("Click to start server...", Colors_White);
				}
				
				if (glui::Button("Start Server", glm::vec4(0.2f, 0.6f, 1.0f, 1.0f))) {
					if (isServerRunning()) {
						std::cout << "Server is already running! Cannot start another server." << std::endl;
					} else {
						std::thread t([](){ serverFunction(7778); });
						t.detach();
						
						// Wait a bit for server to start
						std::this_thread::sleep_for(std::chrono::milliseconds(500));
						
						resetClient();
						strcpy(ip, "127.0.0.1"); // Set IP to localhost
						currentPort = 7778;  // Set port for connection
						state = 2;
					}
				}
				
				if (glui::Button("Back", glm::vec4(0.15f, 0.15f, 0.2f, 0.95f))) {
					g_accountUI->setState(UIState::MAIN_MENU);
				}
			}
			else if (uiState == UIState::JOIN_SERVER && g_accountUI->getIsLoggedIn()) {
				// Set player name from logged in account
				strncpy(name, g_accountUI->getCurrentUsername().c_str(), playerNameSize - 1);
				name[playerNameSize - 1] = '\0';
				
				// Show join server UI
				glui::Space(20);
				glui::Text("=== Join Game Server ===", Colors_White);
				glui::Text("Enter server IP:", Colors_White);
				glui::InputText("##server_ip", ip, sizeof(ip));
				
				if (glui::Button("Join", glm::vec4(0.2f, 0.6f, 1.0f, 1.0f))) {
					resetClient();
					currentPort = 7778;  // Default port for manual join
					state = 1;
				}
				
				if (glui::Button("Back", glm::vec4(0.15f, 0.15f, 0.2f, 0.95f))) {
					g_accountUI->setState(UIState::MAIN_MENU);
				}
			}
			
			glui::Space(20);
			
			// Always show exit button
			if (glui::Button("Exit Game", glm::vec4(0.9f, 0.2f, 0.2f, 1.0f))) {
				return 0;
			}
		} else {
			// Fallback to old UI if account system not initialized
			glui::Text("Multi player game", Colors_White);
			glui::Text("Enter your name:", Colors_White);

			glui::InputText("Enter name##1", name, sizeof(name));
			
			glui::BeginMenu("Host server", glm::vec4(0, 0, 0, 0), {});
				if (isServerRunning()) {
					glui::Text("Server is already running!", Colors_White);
				}
				if (glui::Button("start", glm::vec4(0, 0, 0, 0)))
				{
					if (!isServerRunning()) {
						std::thread t([](){ serverFunction(7778); });
						t.detach();
						std::this_thread::sleep_for(std::chrono::milliseconds(500));
						resetClient();
						strcpy(ip, "127.0.0.1");
						currentPort = 7778;  // Set port for connection
						state = 2;
					}
				}
			glui::EndMenu();
			glui::BeginMenu("Join server", glm::vec4(0, 0, 0, 0), {});
				glui::Text("enter ip: ", Colors_White);
				glui::InputText("input ip", ip, sizeof(ip));
				if (glui::Button("join", glm::vec4(0, 0, 0, 0)))
				{
					resetClient();
					currentPort = 7778;  // Default port for manual join
					state = 1;
				}
			glui::EndMenu();

			if (glui::Button("Exit", glm::vec4(0, 0, 0, 0)))
			{
				return 0;
			}
		}

		glui::renderFrame(renderer, textures.font, platform::getRelMousePosition(),
			platform::isLMousePressed(), platform::isLMouseHeld(), platform::isLMouseReleased(),
			platform::isKeyReleased(platform::Button::Escape), platform::getTypedInput(), deltaTime);

	}
	else if (state == 1 || state == 2)
	{
		// Check for ESC key to toggle pause menu BEFORE glui processes input
		// This must be done before any glui rendering to prevent input consumption
		static bool escWasPressed = false;
		bool escPressed = platform::isKeyPressedOn(platform::Button::Escape);
		
		// Debug output
		if (escPressed) {
			std::cout << "ESC key detected! isPaused=" << isPaused << std::endl;
		}
		
		if (escPressed && !escWasPressed) {
			isPaused = !isPaused;
			std::cout << "Toggling pause state to: " << isPaused << std::endl;
		}
		escWasPressed = escPressed;
		
		if (!isPaused) {
			// Check if client has been disconnected (returns to menu)
			extern bool joined;
			
			if (wasJoined && !joined) {
				// Client was disconnected from server - return to menu
				std::cout << "Client disconnected - returning to menu" << std::endl;
				wasJoined = false;
				state = 0;
				
				// Stop broadcasting if hosting
				if (g_lanDiscovery && g_lanDiscovery->isBroadcasting()) {
					g_lanDiscovery->stopBroadcasting();
				}
				
				// Reset room UI state completely
				if (g_roomUI) {
					g_roomUI->setState(RoomUIState::NONE);
				}
				
				// Reset room UI initialization flag so callbacks get re-setup
				roomUIInitialized = false;
				
				if (g_accountUI) {
					g_accountUI->setState(UIState::MAIN_MENU);
				}
				
				resetClient();
			} else {
				// Normal gameplay - just run the game
				wasJoined = joined;
				clientFunction(deltaTime, renderer, textures, ip, name, currentPort);
			}
		} else {
			// PAUSED STATE - Render pause menu
			
			// First, render game in background (frozen)
			clientFunction(0, renderer, textures, ip, name, currentPort);
			
			// Now render pause menu overlay using glui
			glui::Text("=== GAME PAUSED ===", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
			glui::Space(30);
			
			if (glui::Button("Resume Game", glm::vec4(0.2f, 0.6f, 1.0f, 1.0f))) {
				isPaused = false;
				std::cout << "Resume button clicked" << std::endl;
			}
			
			glui::Space(15);
			
			if (glui::Button("Leave Match", glm::vec4(0.9f, 0.6f, 0.2f, 1.0f))) {
				std::cout << "Leave Match button clicked" << std::endl;
				
				// Disconnect from server
				// Example with data
				
				closeFunction(*g_accountManager);
				resetClient();
				
				// Stop broadcasting if hosting
				if (g_lanDiscovery && g_lanDiscovery->isBroadcasting()) {
					g_lanDiscovery->stopBroadcasting();
				}
				
				// Close server if hosting (state == 2)
				if (state == 2) {
					// Close the specific server on current port
					closeServerByPort(currentPort);
					
					// Stop the room in MultiRoomManager if it exists
					if (g_multiRoomManager) {
						auto activeRooms = g_multiRoomManager->getActiveRooms();
						for (const auto& room : activeRooms) {
							if (room.port == currentPort) {
								g_multiRoomManager->stopRoom(room.slotId);
								std::cout << "Stopped room on port " << currentPort << std::endl;
								break;
							}
						}
					}
				}
				
				// Return to main menu
				isPaused = false;
				state = 0;
				
				// Reset room UI state completely (same as disconnect handler)
				if (g_roomUI) {
					g_roomUI->setState(RoomUIState::NONE);
				}
				roomUIInitialized = false;
				wasJoined = false;  // Reset so next room creation doesn't trigger false disconnect
				
				// Reset account UI state if available
				if (g_accountUI) {
					g_accountUI->setState(UIState::MAIN_MENU);
				}
				
				std::cout << "Left match and returned to main menu" << std::endl;
			}
			
			glui::Space(15);
			
			if (glui::Button("Exit Game", glm::vec4(0.9f, 0.2f, 0.2f, 1.0f))) {
				std::cout << "Exit Game button clicked" << std::endl;
				return 0;  // Exit the entire game
			}
			
			// Render glui frame - pass false for ESC key to prevent glui from handling it
			glui::renderFrame(renderer, textures.font, platform::getRelMousePosition(),
				platform::isLMousePressed(), platform::isLMouseHeld(), platform::isLMouseReleased(),
				false,  // Don't pass ESC to glui - we handle it ourselves
				platform::getTypedInput(), deltaTime);
		}
	}

	
#pragma region set finishing stuff
	renderer.flush();

	return true;
#pragma endregion

}

void closeGame()
{
	closeServer();
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	closeFunction(*g_accountManager);
	
	// Cleanup account system
	if (g_accountUI) {
		delete g_accountUI;
		g_accountUI = nullptr;
	}
	
	if (g_sessionManager) {
		delete g_sessionManager;
		g_sessionManager = nullptr;
	}
	
	if (g_accountManager) {
		g_accountManager->shutdown();
		delete g_accountManager;
		g_accountManager = nullptr;
	}
	
	// Cleanup room system
	if (g_roomUI) {
		delete g_roomUI;
		g_roomUI = nullptr;
	}
	
	if (g_roomHandler) {
		delete g_roomHandler;
		g_roomHandler = nullptr;
	}
	
	if (g_roomManager) {
		delete g_roomManager;
		g_roomManager = nullptr;
	}
	
	// Cleanup LAN discovery
	if (g_lanDiscovery) {
		g_lanDiscovery->stopBroadcasting();
		g_lanDiscovery->stopListening();
		delete g_lanDiscovery;
		g_lanDiscovery = nullptr;
	}
	
	// Cleanup multi-room manager (stops all rooms)
	if (g_multiRoomManager) {
		delete g_multiRoomManager;
		g_multiRoomManager = nullptr;
	}
}

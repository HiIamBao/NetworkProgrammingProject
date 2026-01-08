#pragma once
#include <enet/enet.h>
#include <gl2d/gl2d.h>
#include "Phisics.h"
#include <AccountManager.h>
struct Textures;

void serverFunction(int port = 7778, int gameMode = 0, int mapId = 0);  // Now accepts port, gameMode, and mapId parameters with defaults

// Server state management
bool isServerRunning();  // Check if any server is running
bool isServerRunning(int port);  // Check if server is running on specific port
void resetServerState();
void closeServer();  // Close all servers
void closeServerByPort(int port);  // Close specific server by port

void clientFunction(float deltaTime, gl2d::Renderer2D &renderer, Textures textures, std::string ip, char *playerName, int port = 7778);
void resetClient();
void closeFunction(AccountManager &accountManager);
void setClientAccountManager(AccountManager* accMgr);  // For recording match results

struct Textures
{
	gl2d::Texture sprites;
	gl2d::Texture character;
	gl2d::Texture medKit;
	gl2d::Texture battery;
	gl2d::Texture cross;
	gl2d::Font font;
	gl2d::Texture accountBackground;
};
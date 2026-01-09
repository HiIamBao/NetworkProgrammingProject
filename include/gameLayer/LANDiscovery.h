#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstring>

// LAN server discovery using UDP broadcast
// Servers broadcast their presence, clients listen for broadcasts

struct DiscoveredServer {
    std::string serverName;
    std::string hostName;
    std::string ipAddress;
    int port;
    int playerCount;
    int maxPlayers;
    int gameMode;
    int mapId;
    uint64_t lastSeen;  // Timestamp
};

class LANDiscovery {
public:
    LANDiscovery();
    ~LANDiscovery();
    
    // Server functions
    void startBroadcasting(const std::string& serverName, const std::string& hostName, int port, int gameMode = 0, int mapId = 0);
    void stopBroadcasting();
    void updateServerInfo(int playerCount, int maxPlayers);
    
    // Client functions
    void startListening();
    void stopListening();
    std::vector<DiscoveredServer> getDiscoveredServers();
    void clearDiscoveredServers();
    
    // Check if broadcasting or listening
    bool isBroadcasting() const { return broadcasting.load(); }
    bool isListening() const { return listening.load(); }
    
private:
    // Broadcasting (server side)
    std::atomic<bool> broadcasting;
    std::thread broadcastThread;
    void broadcastLoop();
    
    // Listening (client side)
    std::atomic<bool> listening;
    std::thread listenThread;
    void listenLoop();
    
    // Server info to broadcast
    std::string serverName;
    std::string hostName;
    int serverPort;
    int currentPlayers;
    int maxPlayers;
    int gameMode;
    int mapId;
    std::mutex serverInfoMutex;
    
    // Discovered servers
    std::vector<DiscoveredServer> discoveredServers;
    std::mutex discoveryMutex;
    
    // Network constants
    static constexpr int BROADCAST_PORT = 7779;  // Different from game port
    static constexpr int BROADCAST_INTERVAL_MS = 2000;  // Broadcast every 2 seconds
    static constexpr int SERVER_TIMEOUT_MS = 10000;  // Consider server gone after 10 seconds
    
    // Helper functions
    void cleanupOldServers();
    uint64_t getCurrentTimeMs();
    void broadcastToTailscalePeers(const char* message, int length);
};

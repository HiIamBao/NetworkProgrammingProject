#include "LANDiscovery.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <algorithm>

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

LANDiscovery::LANDiscovery() 
    : broadcasting(false)
    , listening(false)
    , serverPort(7778)
    , currentPlayers(0)
    , maxPlayers(4)
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

LANDiscovery::~LANDiscovery() {
    stopBroadcasting();
    stopListening();
    
#ifdef _WIN32
    WSACleanup();
#endif
}

uint64_t LANDiscovery::getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void LANDiscovery::startBroadcasting(const std::string& name, const std::string& host, int port) {
    if (broadcasting.load()) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(serverInfoMutex);
        serverName = name;
        hostName = host;
        serverPort = port;
    }
    
    broadcasting = true;
    broadcastThread = std::thread(&LANDiscovery::broadcastLoop, this);
}

void LANDiscovery::stopBroadcasting() {
    if (!broadcasting.load()) {
        return;
    }
    
    broadcasting = false;
    if (broadcastThread.joinable()) {
        broadcastThread.join();
    }
}

void LANDiscovery::updateServerInfo(int players, int maxPl) {
    std::lock_guard<std::mutex> lock(serverInfoMutex);
    currentPlayers = players;
    maxPlayers = maxPl;
}

void LANDiscovery::broadcastLoop() {
    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Failed to create broadcast socket" << std::endl;
        return;
    }
    
    // Enable broadcast
    int broadcastEnable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastEnable, sizeof(broadcastEnable)) < 0) {
        std::cerr << "Failed to enable broadcast" << std::endl;
        closesocket(sockfd);
        return;
    }
    
    sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(BROADCAST_PORT);
    broadcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    
    std::cout << "LAN Discovery: Broadcasting server on port " << BROADCAST_PORT << std::endl;
    
    while (broadcasting.load()) {
        // Prepare broadcast message
        char message[256];
        {
            std::lock_guard<std::mutex> lock(serverInfoMutex);
            snprintf(message, sizeof(message), "GAMESERVER|%s|%s|%d|%d|%d", 
                     serverName.c_str(), 
                     hostName.c_str(),
                     serverPort,
                     currentPlayers,
                     maxPlayers);
        }
        
        // Send broadcast
        sendto(sockfd, message, strlen(message), 0, 
               (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
        
        // Wait before next broadcast
        std::this_thread::sleep_for(std::chrono::milliseconds(BROADCAST_INTERVAL_MS));
    }
    
    closesocket(sockfd);
    std::cout << "LAN Discovery: Stopped broadcasting" << std::endl;
}

void LANDiscovery::startListening() {
    if (listening.load()) {
        std::cout << "LAN Discovery: Already listening, skipping..." << std::endl;
        return;
    }
    
    std::cout << "LAN Discovery: Starting listen thread..." << std::endl;
    listening = true;
    
    try {
        listenThread = std::thread(&LANDiscovery::listenLoop, this);
        std::cout << "LAN Discovery: Listen thread started successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "LAN Discovery: Failed to start listen thread: " << e.what() << std::endl;
        listening = false;
    }
}

void LANDiscovery::stopListening() {
    if (!listening.load()) {
        return;
    }
    
    std::cout << "LAN Discovery: Stopping listening..." << std::endl;
    listening = false;
    
    // Give thread time to exit gracefully
    if (listenThread.joinable()) {
        listenThread.join();
    }
    
    // Clear discovered servers
    {
        std::lock_guard<std::mutex> lock(discoveryMutex);
        discoveredServers.clear();
    }
}

void LANDiscovery::listenLoop() {
    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Failed to create listen socket" << std::endl;
        listening = false;
        return;
    }
    
    // Enable address reuse to prevent "Address already in use" errors
    int reuseAddr = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr)) < 0) {
        std::cerr << "Warning: Failed to set SO_REUSEADDR" << std::endl;
    }
    
#ifdef SO_REUSEPORT
    // On Linux, also set SO_REUSEPORT to allow multiple processes to bind to same port
    int reusePort = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char*)&reusePort, sizeof(reusePort));
#endif
    
    // Set socket to non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sockfd, FIONBIO, &mode);
#else
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
#endif
    
    // Bind to broadcast port
    sockaddr_in listenAddr;
    memset(&listenAddr, 0, sizeof(listenAddr));
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_port = htons(BROADCAST_PORT);
    listenAddr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sockfd, (sockaddr*)&listenAddr, sizeof(listenAddr)) < 0) {
        std::cerr << "Failed to bind listen socket to port " << BROADCAST_PORT << std::endl;
        std::cerr << "This usually means another instance is running or port is in use" << std::endl;
        closesocket(sockfd);
        listening = false;
        return;
    }
    
    std::cout << "LAN Discovery: Listening for servers on port " << BROADCAST_PORT << std::endl;
    std::cout << "LAN Discovery: Listen thread running successfully" << std::endl;
    
    char buffer[256];
    sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);
    
    while (listening.load()) {
        int received = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                (sockaddr*)&senderAddr, &senderAddrLen);
        
        if (received > 0) {
            buffer[received] = '\0';
            
            // Parse message: "GAMESERVER|serverName|hostName|port|players|maxPlayers"
            char* token = strtok(buffer, "|");
            if (token && strcmp(token, "GAMESERVER") == 0) {
                DiscoveredServer server;
                
                token = strtok(nullptr, "|");
                if (token) server.serverName = token;
                
                token = strtok(nullptr, "|");
                if (token) server.hostName = token;
                
                token = strtok(nullptr, "|");
                if (token) server.port = atoi(token);
                
                token = strtok(nullptr, "|");
                if (token) server.playerCount = atoi(token);
                
                token = strtok(nullptr, "|");
                if (token) server.maxPlayers = atoi(token);
                
                // Get IP address from sender
                server.ipAddress = inet_ntoa(senderAddr.sin_addr);
                server.lastSeen = getCurrentTimeMs();
                
                // Add or update server in list
                std::lock_guard<std::mutex> lock(discoveryMutex);
                
                // Check if server already exists
                bool found = false;
                for (auto& existing : discoveredServers) {
                    if (existing.ipAddress == server.ipAddress && existing.port == server.port) {
                        // Update existing server
                        existing = server;
                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    discoveredServers.push_back(server);
                    std::cout << "LAN Discovery: Found server '" << server.serverName 
                              << "' at " << server.ipAddress << ":" << server.port << std::endl;
                }
            }
        }
        
        // Cleanup old servers periodically
        cleanupOldServers();
        
        // Sleep a bit to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    closesocket(sockfd);
    std::cout << "LAN Discovery: Stopped listening" << std::endl;
}

void LANDiscovery::cleanupOldServers() {
    // NOTE: This function expects the mutex to already be locked by the caller!
    // Do NOT lock the mutex here to avoid recursive locking/deadlock
    uint64_t now = getCurrentTimeMs();
    
    discoveredServers.erase(
        std::remove_if(discoveredServers.begin(), discoveredServers.end(),
            [now](const DiscoveredServer& server) {
                return (now - server.lastSeen) > SERVER_TIMEOUT_MS;
            }),
        discoveredServers.end()
    );
}

std::vector<DiscoveredServer> LANDiscovery::getDiscoveredServers() {
    std::lock_guard<std::mutex> lock(discoveryMutex);
    cleanupOldServers();  // Safe to call since we already have the lock
    return discoveredServers;
}

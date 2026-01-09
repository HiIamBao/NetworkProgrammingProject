#include "LANDiscovery.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <vector>
#include <sstream>
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
    #include <ifaddrs.h>
    #include <net/if.h>
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

void LANDiscovery::startBroadcasting(const std::string& name, const std::string& host, int port, int mode, int map) {
    if (broadcasting.load()) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(serverInfoMutex);
        serverName = name;
        hostName = host;
        serverPort = port;
        gameMode = mode;
        mapId = map;
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
            snprintf(message, sizeof(message), "GAMESERVER|%s|%s|%d|%d|%d|%d|%d", 
                     serverName.c_str(), 
                     hostName.c_str(),
                     serverPort,
                     currentPlayers,
                     maxPlayers,
                     gameMode,
                     mapId);
        }
        
#ifdef _WIN32
        // Windows: Send to global broadcast address
        // (For robust multi-interface support on Windows, GetAdaptersAddresses is needed,
        // but this simple fallback usually works if the primary interface is correct)
         sendto(sockfd, message, strlen(message), 0, 
               (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
#else
        // Linux/Unix: specific broadcast to each interface
        struct ifaddrs *ifaddr, *ifa;
        bool sent_to_any = false;

        if (getifaddrs(&ifaddr) != -1) {
            for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL) continue;
                if (ifa->ifa_addr->sa_family != AF_INET) continue;
                
                // Debug: Print found interface
                // std::cout << "Checking interface: " << ifa->ifa_name << " Flags: " << ifa->ifa_flags << std::endl;

                bool is_up = (ifa->ifa_flags & IFF_UP);
                bool supports_broadcast = (ifa->ifa_flags & IFF_BROADCAST);
                bool is_p2p = (ifa->ifa_flags & IFF_POINTOPOINT);
                bool is_tailscale = (strstr(ifa->ifa_name, "tailscale") != NULL || strstr(ifa->ifa_name, "tun") != NULL);

                // Allow if UP and (Broadcast OR (P2P + Tailscale special handling))
                if (is_up && (supports_broadcast || (is_p2p && is_tailscale))) {
                    
                    sockaddr_in* target_addr = NULL;
                    
                    if (supports_broadcast && ifa->ifa_broadaddr != NULL) {
                        target_addr = (sockaddr_in*)ifa->ifa_broadaddr;
                    } 
                    else if (is_p2p && ifa->ifa_dstaddr != NULL) {
                        // For P2P VPNs, use destination address or specific broadcast logic?
                        // Standard broadcast won't work on strict P2P without a broadcast IP.
                        // Tailscale magic: usually 100.x.y.255 is not standard.
                        // Best effort: Try sending to the "destination" address if it's a P2P link,
                        // OR if it's Tailscale, we might need to rely on the generic broadcast 
                        // if specific addressing fails, BUT the user says it's "unknown".
                        //
                        // However, simply sending to 255.255.255.255 BINDING to this interface might work better.
                        // But sendto() logic here uses specific destination address.
                        
                        // Let's rely on standard broadcast logic first.
                        // If P2P, we often don't have a broadcast address.
                        // But for Tailscale specifically, it often emulates a network.
                        
                        // User reported interface is "unknown", failing checks.
                        // Let's trust ifa_broadaddr if present even if flag is weird,
                        // OR use a fallback address if it's tailscale.
                         target_addr = (sockaddr_in*)ifa->ifa_dstaddr; // Use P2P destination
                    }

                    if (target_addr != NULL) {
                        sockaddr_in if_bcast = *target_addr;
                        if_bcast.sin_port = htons(BROADCAST_PORT);

                        // If it's P2P/Tailscale, we might want to ensure we aren't just unicasting to gateway.
                        // Tailscale supports "MagicDNS" and broadcast if enabled.
                        // Let's try sending to the derived address.
                        
                        sendto(sockfd, message, strlen(message), 0, 
                               (sockaddr*)&if_bcast, sizeof(if_bcast));
                        sent_to_any = true;
                        // std::cout << "Sent broadcast to " << ifa->ifa_name << std::endl;
                    } else if (is_tailscale) {
                         // Fallback for Tailscale: Try to construct a broadcast address from the IP?
                         // Or just send to 255.255.255.255 but force the interface?
                         // setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, ...) is root only usually.
                         //
                         // Let's try to just use the interface's IP but with .255? 
                         // Too risky without mask validation.
                         //
                         // Simpler fix: If it's tailscale, and we can't find broadaddr, 
                         // try to send to 255.255.255.255 but we can't bind to device easily.
                         //
                         // Let's assume ifa_dstaddr implies the other end for P2P.
                    }
                }
            }
            freeifaddrs(ifaddr);
        }

        // Fallback to global broadcast if enumeration failed or no interfaces found
        // Send broadcast
        sendto(sockfd, message, strlen(message), 0, 
               (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));

        // Tailscale Simulated Broadcast (Simulated via Unicast)
        // Only run every ~5 seconds to avoid spamming the shell command
        static uint64_t lastTailscaleBroadcast = 0;
        uint64_t now = getCurrentTimeMs();
        if (now - lastTailscaleBroadcast > 5000) {
            broadcastToTailscalePeers(message, strlen(message));
            lastTailscaleBroadcast = now;
        }
#endif
        
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
            
            // DEBUG: Print everything received
            // std::cout << "DEBUG: Received UDP packet from " << inet_ntoa(senderAddr.sin_addr) 
            //           << ": " << buffer << std::endl;
            
            // Parse message: "GAMESERVER|serverName|hostName|port|players|maxPlayers|gameMode|mapId"
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
                
                token = strtok(nullptr, "|");
                if (token) server.gameMode = atoi(token);
                else server.gameMode = 0;  // Default to Deathmatch
                
                token = strtok(nullptr, "|");
                if (token) server.mapId = atoi(token);
                else server.mapId = 0;  // Default to map 0
                
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

void LANDiscovery::clearDiscoveredServers() {
    discoveredServers.clear();
}

void LANDiscovery::broadcastToTailscalePeers(const char* message, int length)
{
#ifdef _WIN32
    // Windows implementation could use 'tailscale status' via _popen if needed
    // For now, we'll skip Windows specific implementation
    return; 
#else
    // Linux implementation
    FILE* pipe = popen("tailscale status", "r");
    if (!pipe) {
        // std::cerr << "Failed to run tailscale status" << std::endl;
        return;
    }

    char buffer[256];
    int peerCount = 0;

    // Create a temporary socket for sending
    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        pclose(pipe);
        return;
    }

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        // Line format example:
        // 100.102.199.13  machine-a            hungletatdac12345@  linux  idle, tx 12164 rx 11356
        
        // Extract IP (first token)
        char* ipToken = strtok(buffer, " \t");
        if (ipToken) {
            // Basic validation: starts with 100. (common Tailscale range) or just valid IP check
            // Tailscale IPs are in 100.64.0.0/10 usually.
            
            // Try to convert to sockaddr
            sockaddr_in peerAddr;
            memset(&peerAddr, 0, sizeof(peerAddr));
            peerAddr.sin_family = AF_INET;
            peerAddr.sin_port = htons(BROADCAST_PORT);
            
            if (inet_pton(AF_INET, ipToken, &peerAddr.sin_addr) == 1) {
                // Check if it's not our own IP (optimization, though sendto usually handles loopback fine)
                // For now, just send.
                
                sendto(sockfd, message, length, 0, 
                       (sockaddr*)&peerAddr, sizeof(peerAddr));
                peerCount++;
                
                // std::cout << "Tailscale Broadcast -> " << ipToken << std::endl;
            }
        }
    }

    pclose(pipe);
    closesocket(sockfd);
    
    if (peerCount > 0) {
        // std::cout << "Simulated broadcast to " << peerCount << " Tailscale peers." << std::endl;
    }
#endif
}

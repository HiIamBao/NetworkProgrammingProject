#pragma once
#include <enet/enet.h>
#include <string>
#include <cstdint>

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    bool connect(const std::string& ip, int port, const char* playerName);
    void disconnect();
    
    // Returns true if an event was polled
    bool pollEvent(ENetEvent& event);
    
    void sendPacket(const void* data, size_t size, bool reliable);
    
    bool isConnected() const { return connected; }
    int32_t getClientId() const { return cid; }
    
    // Set client ID (usually received from server handshake)
    void setClientId(int32_t id) { cid = id; }
    void setConnected(bool status) { connected = status; }

private:
    ENetHost* client = nullptr;
    ENetPeer* server = nullptr;
    int32_t cid = -1;
    bool connected = false;
};

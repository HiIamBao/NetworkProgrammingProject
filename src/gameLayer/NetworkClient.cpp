#include "NetworkClient.h"
#include <iostream>
#include <cstring>
#include "packet.h"

NetworkClient::NetworkClient() {
    // ENet should be initialized globally, but we can double check or assume it is
}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect(const std::string& ip, int port, const char* playerName) {
    if (client) {
        enet_host_destroy(client);
        client = nullptr;
    }

    client = enet_host_create(NULL, 1, 2, 0, 0);
    if (client == NULL) {
        std::cout << "An error occurred while trying to create an ENet client host." << std::endl;
        return false;
    }

    ENetAddress address;
    enet_address_set_host(&address, ip.c_str());
    address.port = port;

    server = enet_host_connect(client, &address, 2, 0);
    if (server == NULL) {
        std::cout << "No available peers for initiating an ENet connection." << std::endl;
        enet_host_destroy(client);
        client = nullptr;
        return false;
    }

    // Wait for connection
    ENetEvent event;
    if (enet_host_service(client, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT)
    {
        std::cout << "Connection to " << ip << ":" << port << " succeeded." << std::endl;
        connected = true;
    }
    else
    {
        enet_peer_reset(server);
        std::cout << "Connection to " << ip << ":" << port << " failed." << std::endl;
        enet_host_destroy(client);
        client = nullptr;
        return false;
    }

    return true;
}

void NetworkClient::disconnect() {
    if (server) {
        enet_peer_disconnect(server, 0);
        
        // Flush events
        if (client) {
            enet_host_flush(client);
        }
        
        server = nullptr;
    }
    
    if (client) {
        enet_host_destroy(client);
        client = nullptr;
    }
    
    connected = false;
    cid = -1;
}

bool NetworkClient::pollEvent(ENetEvent& event) {
    if (!client) return false;
    return enet_host_service(client, &event, 0) > 0;
}

void NetworkClient::sendPacket(const void* data, size_t size, bool reliable) {
    if (!server) return;
    
    ENetPacket* packet = enet_packet_create(data, size, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    enet_peer_send(server, 0, packet);
}

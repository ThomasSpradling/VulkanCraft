#pragma once

#include <array>
#include <glm/glm.hpp>
#include "../Network/Address.h"

// Sockets
#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>

class GameServer {
public:
    void Initialize();
    void Run();
    void Update(float delta_time);
private:
    struct PlayerState {
        glm::vec3 position { 0.0f, 0.0f, 5.0f };
    };
    
    struct PlayerInput {
        glm::vec3 movement_direction { 0.0f };
    };

    struct Client {
        int id = -1;

        NetworkAddress client_address;
        size_t client_address_length;
        double last_timestamp;

        PlayerState player_state;
        PlayerInput player_input;
    };
private:
    const int UPDATE_RATE = 60; // Hz
    const int CLIENT_TIMEOUT = 5000; // ms

    double m_network_send_timer = 0.0;
    const int NETWORK_SNAPSHOT_RATE = 30; // Hz

    SOCKET m_socket;

    static const int MAX_CLIENTS = 32;
    std::array<Client, MAX_CLIENTS> m_clients {};

    addrinfo *m_address_info = nullptr;
private:
    void Tick(float delta_time);
    void ReceiveNetworkPackets();
    void SendNetworkPackets();
};

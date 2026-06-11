#pragma once

#include <glm/glm.hpp>

// Sockets
#include <WinSock2.h>
#include <WS2tcpip.h>

class GameServer {
public:
    void Initialize();
    void Run();
    void Update(float delta_time);
private:
    glm::vec3 m_current_position { 0.0f, 0.0f, 5.0f };
    glm::vec3 m_current_movement_direction { 0.0f, 0.0f, 0.0f };
    const int UPDATE_RATE = 60; // Hz

    double m_network_send_timer = 0.0;
    const int NETWORK_SNAPSHOT_RATE = 30; // Hz

    SOCKET m_socket;
    bool m_connected_client = false;

    sockaddr m_client_addr{};
    int m_client_addr_length = sizeof(m_client_addr);
private:
    void Tick(float delta_time);
    void ReceiveNetworkPackets();
    void SendNetworkPackets();
};

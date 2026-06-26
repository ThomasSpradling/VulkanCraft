#pragma once

#include <array>
#include <glm/glm.hpp>
#include <memory>
#include "../Platform/Sockets/SocketAPI.h"
#include "../Network/NetworkHost.h"

#include "../Utils/NonCopyable.h"
#include "../Utils/NonMovable.h"

class GameServer : public NonCopyable, public NonMovable {
public:
    GameServer();
    ~GameServer() = default;

    void Initialize();
    void Run();
    void Update(float delta_time);
private:
    struct PlayerState {
        float yaw = 90.0f;
        float pitch = 0.0f;
        glm::vec3 position { 0.0f, 11.0f, 5.0f };
    };
    
    struct PlayerInput {
        glm::vec3 movement_direction { 0.0f };
        float yaw = 90.0f;
        float pitch = 0.0f;
    };

    struct Client {
        int id = -1;

        // NetworkAddress client_address;
        size_t client_address_length = 0;
        double last_timestamp = 0.0;

        PlayerState player_state;
        PlayerInput player_input;
    };
private:
    const int UPDATE_RATE = 60; // Hz
    const int CLIENT_TIMEOUT = 5000; // ms

    double m_network_send_timer = 0.0;
    const int NETWORK_SNAPSHOT_RATE = 10; // Hz

    static const int MAX_CLIENTS = 32;
    std::array<Client, MAX_CLIENTS> m_clients {};

    std::unique_ptr<SocketAPI> m_socket_api {};
    std::unique_ptr<NetworkHost> m_server {};

    const ChannelId m_basic_channel = 1;
private:
    void Tick(float delta_time);
    void ReceiveNetworkPackets();
    void SendNetworkPackets();
};

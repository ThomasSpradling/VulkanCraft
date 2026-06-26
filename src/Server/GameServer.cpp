#include "GameServer.h"
#include <array>
#include <chrono>

#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <ratio>
#include <string>
#include <thread>
#include "../Network/Protocol.h"
#include "../Network/Packets/ClientPackets.h"
#include "../Network/Packets/ServerPackets.h"
#include "../Network/Packets/SystemPackets.h"
// #include "../Network/Packets.h"

GameServer::GameServer() {
    m_socket_api = std::make_unique<SocketAPI>();
    m_socket_api->Initialize();
    
    NetworkAddress address = NetworkAddress::Any(PROTOCOL_PORT);
    m_server = std::make_unique<NetworkHost>(HostType::Server, address, 2);

    std::cout << "Server running at address " << address.AddressName() << " on port " << address.Port() << "\n";
}

void GameServer::Initialize() {



    // WSADATA winsock_data;
    // if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
    //     std::cerr << "WSA starup failed!";
    // }

    // addrinfo hints = {
    //     .ai_flags = AI_PASSIVE, // connect to localhost for now
    //     .ai_family = AF_INET,
    //     .ai_socktype = SOCK_DGRAM,
    // };

    // addrinfo *address_info;
    // if (getaddrinfo(nullptr, std::to_string(PROTOCOL_PORT).data(), &hints, &address_info) != 0) {
    //     std::cerr << "failed to create addrinfo\n";
    // }
    // m_address_info = address_info;

    // m_socket = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
    // if (m_socket == INVALID_SOCKET) {
    //     std::cerr << "Invalid socket!\n";
    // }

    // if (bind(m_socket, address_info->ai_addr, address_info->ai_addrlen) == SOCKET_ERROR) {
    //     std::cerr << "Failed to bind!\n";
    // }

    // // std::cout << "Server listening at address "
    // //     << GetHostName(address_info->ai_family, *(address_info->ai_addr))
    // //     << " on port "
    // //     << PROTOCOL_PORT
    // //     << ".\n";

    // u_long enabled = 1;
    // if (ioctlsocket(m_socket, FIONBIO, &enabled) == SOCKET_ERROR) {
    //     std::cerr << "Failed to set server socket non-blocking: " << WSAGetLastError() << "\n";
    // }

}

void GameServer::Run() {
    using clock = std::chrono::steady_clock;

    Initialize();
    auto previous_time = clock::now();

    double update_accum = 0.0;

    const double UPDATE_STEP_TIME = 1000.0 / UPDATE_RATE;
    constexpr double MAX_FRAME_WAIT_TIME_MS = 250.0;

    while (true) {
        auto current_time = clock::now();

        std::chrono::duration<double, std::milli> delta_time = current_time - previous_time;

        previous_time = current_time;

        double frame_ms = std::min(delta_time.count(), MAX_FRAME_WAIT_TIME_MS);

        update_accum += frame_ms;

        while (update_accum >= UPDATE_STEP_TIME) {
            Update(static_cast<float>(UPDATE_STEP_TIME));
            update_accum -= UPDATE_STEP_TIME;
        }

        if (update_accum < UPDATE_STEP_TIME) {
            double sleep_ms = UPDATE_STEP_TIME - update_accum;

            if (sleep_ms > 1.0) {
                std::this_thread::sleep_for(
                    std::chrono::duration<double, std::milli>(sleep_ms)
                );
            }
        }
    }

    std::cout << "Server disconnected.\n";
}

void GameServer::Update(float delta_time) {
    Tick(delta_time);

    m_network_send_timer += delta_time;
    if (m_network_send_timer >= 1000.0f / NETWORK_SNAPSHOT_RATE) {
        m_network_send_timer = 0.0;
        SendNetworkPackets();
        m_server->Update(1000.0f / NETWORK_SNAPSHOT_RATE);
        ReceiveNetworkPackets();
    }
}

void GameServer::Tick(float delta_time) {
    for (Client &client : m_clients) {
        if (client.id == -1) {
            continue;
        }

        client.player_state.position += client.player_input.movement_direction * 0.01f * delta_time;
        client.player_state.yaw = client.player_input.yaw;
        client.player_state.pitch = client.player_input.pitch;

        client.player_input.movement_direction = glm::vec3(0.0f);
        client.last_timestamp += delta_time;

        if (client.last_timestamp > CLIENT_TIMEOUT) {
            std::cout << "Client " << client.id << " timed out!\n";
            // client.client_address = {};
            client.id = -1;
        }
    }
}

void GameServer::ReceiveNetworkPackets() {
    NetworkCommand command;
    while (m_server->PollNetworkCommand(command)) {
        PeerId peer_id = command.peer;
        if (command.type == NetworkCommandType::Connect) {
            std::cout << "Client " << peer_id << " has connected!\n";
            continue;
        }

        if (command.type == NetworkCommandType::Disconnect) {
            std::cout << "Client " << peer_id << " has disconnected!\n";
            continue;
        }

        if (command.type != NetworkCommandType::ReceivePacket)
            continue;
        
        Packet &packet = *command.packet;
        switch (packet.Type()) {
            case PacketType::Test: {
                auto *test_packet = dynamic_cast<TestPacket *>(&packet);
                std::cout << "Received packet from Client " << peer_id << ": " << test_packet->value << "\n";
                m_server->SendPacket(peer_id, m_basic_channel, std::move(command.packet));
                break;
            }
            default:
                break;
        }
    }
}

void GameServer::SendNetworkPackets() {
}

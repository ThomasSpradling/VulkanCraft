#include "GameServer.h"
#include <array>
#include <chrono>

#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include "../Network/Packets.h"

void GameServer::Initialize() {
    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
        std::cerr << "WSA starup failed!";
    }

    addrinfo hints = {
        .ai_flags = AI_PASSIVE, // connect to localhost for now
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };

    addrinfo *address_info;
    if (getaddrinfo(nullptr, std::to_string(PROTOCOL_PORT).data(), &hints, &address_info) != 0) {
        std::cerr << "failed to create addrinfo\n";
    }
    m_address_info = address_info;

    m_socket = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Invalid socket!\n";
    }

    if (bind(m_socket, address_info->ai_addr, address_info->ai_addrlen) == SOCKET_ERROR) {
        std::cerr << "Failed to bind!\n";
    }

    std::cout << "Server listening at address "
        << GetHostName(address_info->ai_family, *(address_info->ai_addr))
        << " on port "
        << PROTOCOL_PORT
        << ".\n";

    u_long enabled = 1;
    if (ioctlsocket(m_socket, FIONBIO, &enabled) == SOCKET_ERROR) {
        std::cerr << "Failed to set server socket non-blocking: " << WSAGetLastError() << "\n";
    }

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

        std::chrono::duration<double, std::milli> delta_time =
            current_time - previous_time;

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
    ReceiveNetworkPackets();

    Tick(delta_time);

    m_network_send_timer += delta_time;
    if (m_network_send_timer >= 1000.0f / NETWORK_SNAPSHOT_RATE) {
        m_network_send_timer = 0.0;
        SendNetworkPackets();
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
            client.client_address = {};
            client.id = -1;
        }
    }
}

void GameServer::ReceiveNetworkPackets() {
    NetworkBuffer recv_buffer;

    while (true) {
        //// Get a packet ////
        recv_buffer.Resize(512);

        sockaddr sock_client_addr{};
        int client_addr_length = sizeof(sock_client_addr);

        int bytes_received = recvfrom(m_socket, recv_buffer.GetData(), static_cast<int>(recv_buffer.GetSize()), 0, &sock_client_addr, &client_addr_length);
        if (bytes_received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (WSAGetLastError() != WSAEWOULDBLOCK)
                std::cerr << "Server recvfrom failed: " << error << "\n";
            break;
        }
        recv_buffer.Resize(bytes_received);
        NetworkAddress client_addr = FromSockAddr(sock_client_addr);

        //// Process Packet Commands ////
        Packet packet;
        PacketError err = packet.Read(recv_buffer);
        if (err == PacketError::ChecksumError) {
            std::cerr << "Packet dropped due to mismatching checksum.\n";
            continue;
        }

        if (err == PacketError::ChecksumError) {
            std::cerr << "Packet dropped due to serialization error.\n";
            continue;
        }

        switch (packet.packet_type) {
            case PacketType::ClientJoin: {
                int slot = -1;
                for (uint32_t i = 0; i < MAX_CLIENTS; ++i) {
                    if (m_clients[i].id == -1) {
                        slot = i;
                        break;
                    }
                }
                
                NetworkBuffer buffer;
                Packet send_packet {
                    .packet_type = PacketType::JoinResult,
                };

                if (slot == -1) {
                    // Too full!
                    send_packet.packet_data = PacketJoinResult{
                        .is_accepted = false,
                        .player_id = 0,
                    };
                    send_packet.Write(buffer);

                    sendto(m_socket, buffer.GetData(), buffer.GetSize(), 0, &sock_client_addr, client_addr_length);
                } else {
                    std::cout << "Client " << slot << " connected"
                        << " from "
                        << GetHostName(m_address_info->ai_family, sock_client_addr)
                        << " on port "
                        << client_addr.port
                        << ".\n";
                    send_packet.packet_data = PacketJoinResult{
                        .is_accepted = true,
                        .player_id = static_cast<uint32_t>(slot),
                    };
                    send_packet.Write(buffer);

                    if (sendto(m_socket, buffer.GetData(), buffer.GetSize(), 0, &sock_client_addr, client_addr_length) != SOCKET_ERROR) {
                        m_clients[slot].id = slot;
                        m_clients[slot].client_address = client_addr;
                        m_clients[slot].client_address_length = client_addr_length;
                        m_clients[slot].last_timestamp = 0.0f;
                        m_clients[slot].player_state = {};
                        m_clients[slot].player_input = {};
                    }
                }
                break;
            }
            case PacketType::ClientLeave: {
                const auto &data = std::get<PacketClientLeave>(packet.packet_data);
                if (m_clients[data.player_id].client_address == client_addr &&
                    m_clients[data.player_id].id == data.player_id)
                {
                    std::cout << "Client " << data.player_id << " disconnected!\n";
                    m_clients[data.player_id] = {
                        .id = -1,
                    };
                }
                break;
            }
            case PacketType::Heartbeat: {
                const auto &data = std::get<PacketHeartbeat>(packet.packet_data);
                if (m_clients[data.player_id].client_address == client_addr &&
                    m_clients[data.player_id].id == data.player_id)
                {
                    m_clients[data.player_id].last_timestamp = 0.0f;
                }
                break;
            }
            case PacketType::MovePlayer: {
                const auto &data = std::get<PacketMovePlayer>(packet.packet_data);
                
                if (m_clients[data.player_id].client_address == client_addr &&
                    m_clients[data.player_id].id == data.player_id)
                {
                    glm::vec3 movement_direction = data.direction;
                    if (movement_direction != glm::vec3(0.0f)) {
                        movement_direction = glm::normalize(movement_direction);
                    }
                    
                    m_clients[data.player_id].player_input.movement_direction = movement_direction;
                    m_clients[data.player_id].last_timestamp = 0.0f;
                }
                break;
            }
            case PacketType::ChangeView: {
                const auto &data = std::get<PacketChangeView>(packet.packet_data);
                
                if (m_clients[data.player_id].client_address == client_addr &&
                    m_clients[data.player_id].id == data.player_id)
                {   
                    m_clients[data.player_id].player_input.yaw = data.yaw;
                    m_clients[data.player_id].player_input.pitch = data.pitch;
                }
                break;
            }
            case PacketType::Invalid:
            default:
                break;
        }
    }
}

void GameServer::SendNetworkPackets() {
    NetworkBuffer send_buffer;
    Packet packet {
        .packet_type = PacketType::PlayerState,
    };

    std::vector<PacketPlayerState::Data> players {};
    uint32_t count = 0;
    for (Client &client : m_clients) {
        if (client.id != -1) {
            players.push_back({
                .id = static_cast<uint32_t>(client.id),
                .position = client.player_state.position,
                .yaw = client.player_state.yaw,
                .pitch = client.player_state.pitch,
            });
            
            ++count;
        }
    }
    packet.packet_data = PacketPlayerState{
        .count = count,
        .data = players
    };
    packet.Write(send_buffer);

    for (Client &client : m_clients) {
        if (client.id != -1) {
            sockaddr addr = ToSockAddr(client.client_address);

            if (sendto(m_socket, send_buffer.GetData(), send_buffer.GetSize(), 0, &addr, sizeof(addr)) == SOCKET_ERROR) {
                std::cerr << "Failed sendto!\n";
            }
        }
    }
}

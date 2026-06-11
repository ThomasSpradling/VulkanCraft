#include "GameServer.h"
#include <array>
#include <chrono>

#include <iostream>
#include <optional>
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
    if (getaddrinfo(nullptr, "9999", &hints, &address_info) != 0) {
        std::cerr << "failed to create addrinfo\n";
    }

    m_socket = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Invalid socket!\n";
    }

    if (bind(m_socket, address_info->ai_addr, address_info->ai_addrlen) == SOCKET_ERROR) {
        std::cerr << "Failed to bind!\n";
    }

    std::cout << "Set Server Sockets!\n";

    // Look for acknowledgement
    NetworkBuffer buffer;
    buffer.Resize(512);

    sockaddr client_addr;
    int client_addr_length = sizeof(client_addr);
    int bytes_received = recvfrom(m_socket, buffer.GetData(), buffer.GetSize(), 0, &client_addr, &client_addr_length);

    if (bytes_received == SOCKET_ERROR) {
        std::cerr << "Error on recvfrom! " << WSAGetLastError() << std::endl;
    }
    
    Packet recv_packet;
    recv_packet.Read(buffer);
    if (recv_packet.packet_type == PacketType::Ack) {
        m_connected_client = true;
        m_client_addr = client_addr;
        m_client_addr_length = client_addr_length;

        std::cout << "RECEIVED AN ACKNOWLEDGEMENT!\n";

        Packet send_packet {
            .packet_type = PacketType::Ack,
        };

        NetworkBuffer send_buffer;
        send_packet.Write(send_buffer);

        bytes_received = sendto(
            m_socket,
            send_buffer.GetData(),
            static_cast<int>(send_buffer.GetSize()),
            0,
            &m_client_addr,
            m_client_addr_length
        );

        if (bytes_received == SOCKET_ERROR) {
            std::cerr << "Error on send to!\n";
        }
    }

    u_long non_blocking = 1;
    if (ioctlsocket(m_socket, FIONBIO, &non_blocking) == SOCKET_ERROR) {
        std::cerr << "Failed to set server socket non-blocking: " << WSAGetLastError() << "\n";
    }
}

void GameServer::Run() {
    using clock = std::chrono::high_resolution_clock;

    auto previous_time = clock::now();

    double update_accum = 0.0;

    const double UPDATE_STEP_TIME = 1000.0 / UPDATE_RATE;
    constexpr double MAX_FRAME_WAIT_TIME_MS = 250.0;

    Initialize();
    while (true) {
        // poll events

        auto current_time = clock::now();
        std::chrono::duration<double, std::milli> delta_time = current_time - previous_time;
        previous_time = current_time;
        
        update_accum += delta_time.count();

        while (update_accum >= UPDATE_STEP_TIME) {
            Update(UPDATE_STEP_TIME);
            update_accum -= UPDATE_STEP_TIME;
        }
    }
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
    m_current_position += m_current_movement_direction * 0.01f * delta_time;
    m_current_movement_direction = glm::vec3(0.0f);
}

void GameServer::ReceiveNetworkPackets() {
    NetworkBuffer recv_buffer;

    bool accepted_move_packet_this_update = false;

    while (true) {
        recv_buffer.Resize(512);

        sockaddr client_addr{};
        int client_addr_length = sizeof(client_addr);

        int bytes_received = recvfrom(m_socket, recv_buffer.GetData(), static_cast<int>(recv_buffer.GetSize()), 0, &client_addr, &client_addr_length);
        if (bytes_received == SOCKET_ERROR) {
            int error = WSAGetLastError();

            if (error == WSAEWOULDBLOCK) {
                break;
            }

            std::cerr << "Server recvfrom failed: " << error << "\n";
            break;
        }

        recv_buffer.Resize(bytes_received);

        Packet packet;
        packet.Read(recv_buffer);

        switch (packet.packet_type) {
            case PacketType::MovePlayer: {
                if (accepted_move_packet_this_update) {
                    break;
                }

                const auto &packet_data = std::get<PacketMovePlayer>(packet.packet_data);

                glm::vec3 movement_direction = packet_data.direction;

                if (movement_direction != glm::vec3(0.0f)) {
                    movement_direction = glm::normalize(movement_direction);
                }

                m_current_movement_direction = movement_direction;

                m_client_addr = client_addr;
                m_client_addr_length = client_addr_length;

                accepted_move_packet_this_update = true;
                break;
            }

            case PacketType::Ack:
            case PacketType::Invalid:
            default:
                break;
        }
    }
}

void GameServer::SendNetworkPackets() {
    if (!m_connected_client) {
        return;
    }

    Packet packet {
        .packet_type = PacketType::SetPosition,
        .packet_data = PacketSetPosition{
            .position = m_current_position,
        }
    };

    NetworkBuffer send_buffer;
    packet.Write(send_buffer);

    int bytes_sent = sendto(m_socket, send_buffer.GetData(), static_cast<int>(send_buffer.GetSize()), 0, &m_client_addr, m_client_addr_length);

    if (bytes_sent == SOCKET_ERROR) {
        std::cerr << "Server sendto failed: " << WSAGetLastError() << "\n";
    }
}

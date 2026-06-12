#pragma once

#include "NetworkBuffer.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <variant>

#include "Packets/ClientPackets.h"
#include "Packets/ServerPackets.h"
#include "../errors.h"

enum class PacketType : uint8_t {
    Invalid = 0,

    // Client packets
    ClientJoin,
    ClientLeave,
    MovePlayer,
    Heartbeat,

    // Server packets
    JoinResult,
    PlayerState,
};

using PacketData = std::variant<
    std::monostate,
    PacketClientLeave,
    PacketHeartbeat,
    PacketMovePlayer,

    PacketJoinResult,
    PacketPlayerState
>;

struct Packet {
    PacketType packet_type = PacketType::Invalid;
    PacketData packet_data = std::monostate{};

    // Read from buffer into this structure
    inline void Read(NetworkBuffer &buffer) {
        packet_type = static_cast<PacketType>(buffer.ReadByte());

        
        switch (packet_type) {
#define REGISTER_PACKET(pack)                                     \
            case PacketType::pack: {                              \
                auto &data = packet_data.emplace<Packet##pack>(); \
                data.Read(buffer);                                \
                break;                                            \
            }
            
            REGISTER_PACKET(Heartbeat)
            REGISTER_PACKET(ClientLeave)
            REGISTER_PACKET(MovePlayer)
            REGISTER_PACKET(JoinResult)
            REGISTER_PACKET(PlayerState)

            case PacketType::ClientJoin:
            case PacketType::Invalid:
            default:
                packet_data = std::monostate{};
                break;
        }
    }
#undef  REGISTER_PACKET

    // Write to buffer using this structure
    inline void Write(NetworkBuffer &buffer) const {
        buffer.Write(static_cast<uint8_t>(packet_type));

        switch (packet_type) {
#define REGISTER_PACKET(pack)                                                \
            case PacketType::pack: {                                         \
                const auto *data = std::get_if<Packet##pack>(&packet_data);  \
                Assert(data != nullptr, "Packet type mismatch");             \
                data->Write(buffer);                                         \
                break;                                                       \
            }

            REGISTER_PACKET(Heartbeat)
            REGISTER_PACKET(ClientLeave)
            REGISTER_PACKET(MovePlayer)
            REGISTER_PACKET(JoinResult)
            REGISTER_PACKET(PlayerState)

            case PacketType::ClientJoin:
            case PacketType::Invalid:
            default:
                break;
        }

#undef  REGISTER_PACKET
    }
};

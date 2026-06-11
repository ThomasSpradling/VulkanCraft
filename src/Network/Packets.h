#pragma once

#include "NetworkBuffer.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <variant>

enum class PacketType : uint8_t {
    Invalid = 0,
    Ack,
    MovePlayer,
    SetPosition,
};

struct PacketMovePlayer {
    glm::vec3 direction;

    inline void Read(NetworkBuffer &buffer) {
        direction = buffer.ReadVec3();
    }

    inline void Write(NetworkBuffer &buffer) const {
        buffer.Write(direction);
    }
};

struct PacketSetPosition {
    glm::vec3 position;

    inline void Read(NetworkBuffer &buffer) {
        position = buffer.ReadVec3();
    }

    inline void Write(NetworkBuffer &buffer) const {
        buffer.Write(position);
    }
};

using PacketData = std::variant<
    std::monostate,
    PacketMovePlayer,
    PacketSetPosition
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
            
            REGISTER_PACKET(MovePlayer)
            REGISTER_PACKET(SetPosition)

            case PacketType::Ack:
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
#define REGISTER_PACKET(pack)                                           \
            case PacketType::pack: {                                    \
                const auto &data = std::get<Packet##pack>(packet_data); \
                data.Write(buffer);                                     \
                break;                                                  \
            }

            REGISTER_PACKET(MovePlayer)
            REGISTER_PACKET(SetPosition)

            case PacketType::Ack:
            case PacketType::Invalid:
            default:
                break;
        }

#undef  REGISTER_PACKET
    }
};

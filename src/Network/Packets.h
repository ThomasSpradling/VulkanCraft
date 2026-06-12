#pragma once

#include "NetworkBuffer.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <iostream>
#include <variant>
#include <crc32.h> // checksum

#include "Packets/ClientPackets.h"
#include "Packets/ServerPackets.h"
#include "../errors.h"
#include "Protocol.h"

enum class PacketType : uint8_t {
    Invalid = 0,

    // Client packets
    ClientJoin,
    ClientLeave,
    MovePlayer,
    ChangeView,
    Heartbeat,

    // Server packets
    JoinResult,
    PlayerState,
};

enum class PacketError : uint8_t {
    NoError = 0,
    SerializationError,
    ChecksumError,
};

using PacketData = std::variant<
    std::monostate,
    PacketClientLeave,
    PacketHeartbeat,
    PacketMovePlayer,
    PacketChangeView,

    PacketJoinResult,
    PacketPlayerState
>;

/**
 * Packet Structure:
 * 0    DATA_SIZE           (4 bytes)   |- HEADER
 * 4    CHECKSUM            (4 bytes)   | 
 * 8    PACKET_TYPE         (1 byte)    |- DATA
 * 9    SPECIFIC_PACKET_DATA            |
 * ...                                  |
 * x    SERIALIZATION_CHECK (4 bytes)   |- FOOTER
 */
struct Packet {
public:
    PacketType packet_type = PacketType::Invalid;
    PacketData packet_data = std::monostate{};

    const uint32_t PACKET_DATA_BEGIN = 8;   // start of PACKET_TYPE

    // Read from buffer into this structure
    inline PacketError Read(NetworkBuffer &buffer) {
        uint32_t packet_data_size = buffer.ReadInteger();
        uint32_t received_checksum = buffer.ReadInteger();
        packet_type = static_cast<PacketType>(buffer.ReadByte());
        
        // compute checksum on all data not including checksum and serialization check
        uint32_t computed_checksum = crc32(
            buffer.GetRawData() + PACKET_DATA_BEGIN,
            packet_data_size
        );
        if (received_checksum != computed_checksum) {
            std::cout << "Checksum failed!\n";
            return PacketError::ChecksumError;
        }

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
            REGISTER_PACKET(ChangeView)
            REGISTER_PACKET(JoinResult)
            REGISTER_PACKET(PlayerState)

            case PacketType::ClientJoin:
            case PacketType::Invalid:
            default:
                packet_data = std::monostate{};
                break;
        }

        uint32_t serialization_check = buffer.ReadInteger();
        if (serialization_check != SERIALIZATION_CHECK) {
            std::cout << "Serialization check failed!\n";
            return PacketError::SerializationError;
        }

        return PacketError::NoError;
    }
#undef  REGISTER_PACKET

    // Write to buffer using this structure
    inline void Write(NetworkBuffer &buffer) const {
        
        NetworkBuffer packet_data_buffer;
        WriteData(packet_data_buffer);
        
        uint32_t packet_data_size = packet_data_buffer.GetSize();
        uint32_t checksum = crc32(packet_data_buffer.GetRawData(), packet_data_buffer.GetSize());
        
        buffer.Write(packet_data_size);
        buffer.Write(checksum);
        buffer.Insert(packet_data_buffer);
        buffer.Write(SERIALIZATION_CHECK);
    }

private:
    // Write only the data into this buffer, ignoring checksum and serialization
    // check
    inline void WriteData(NetworkBuffer &buffer) const {
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
            REGISTER_PACKET(ChangeView)
            REGISTER_PACKET(JoinResult)
            REGISTER_PACKET(PlayerState)

            case PacketType::ClientJoin:
            case PacketType::Invalid:
            default:
                break;
        }
    }
#undef  REGISTER_PACKET
};

#pragma once

#include "../Platform/Sockets/NetworkBuffer.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <iostream>
#include <variant>
#include <crc32.h> // checksum

#include "Packets/ClientPackets.h"
#include "Packets/ServerPackets.h"
#include "../errors.h"
#include "Packets/SystemPackets.h"
#include "Protocol.h"

enum class PacketType : uint8_t {
    // Reserved
    Invalid = 0,
    Fragment,
    Slice,
    SliceAck,

    // Client packets
    ConnectionRequest,
    DisconnectRequest,
    ClientJoin,
    ClientLeave,
    MovePlayer,
    ChangeView,
    Heartbeat,

    // Server packets
    ConnectionResponse,
    DisconnectResponse,
    JoinResult,
    PlayerState,

    Test,
    LargeTest,
};

enum class PacketError : uint8_t {
    NoError = 0,
    SerializationError,
    SizeError,
    ChecksumError,
};

using PacketData = std::variant<
    std::monostate,
    PacketFragment,
    PacketSlice,
    PacketSliceAck,

    PacketConnectionResponse,
    PacketClientLeave,
    PacketHeartbeat,
    PacketMovePlayer,
    PacketChangeView,

    PacketJoinResult,
    PacketPlayerState,

    PacketLargeTest,
    PacketTest
>;

/**
 * Basic Packet Structure:
 * 0    CHECKSUM            (4 bytes)
 * 4    SEQUENCE            (2 bytes) 
 * --------------------------------------
 * 6    PACKET_TYPE         (1 byte)
 * 7    PACKET_DATA
 * ...
 * --------------------------------------
 * x    SERIALIZATION_CHECK (4 bytes)
 *
 *
 * Fragment Packet Structure:
 * 0    CHECKSUM            (4 bytes)
 * 4    SEQUENCE            (2 bytes) 
 * --------------------------------------
 * 6    <PACKET_FRAGMENT>   (1 byte)
 * 7    FRAGMENT_ID         (2 bytes)
 * 9    NUM_FRAGMENTS       (2 bytes)
 * 11   PACKET_TYPE         (1 byte)
 * 12   FRAGMENT_DATA
 * ...
 * --------------------------------------
 * x    SERIALIZATION_CHECK (4 bytes)
 *
 */
const uint32_t PACKET_DATA_BEGIN = 6;   // Checksum + sequence number
const uint32_t PACKET_DATA_END = 4;     // Serialization

struct Packet {
public:
    PacketType packet_type = PacketType::Invalid;
    uint16_t sequence = 0;
    PacketData packet_data = std::monostate{};
    
    // PacketFragmentHeader fragment_header {};

    // Read from buffer into this structure
    inline PacketError Read(NetworkBuffer &buffer) {
        uint32_t packet_data_size = buffer.GetSize() - PACKET_DATA_BEGIN - PACKET_DATA_END;
        uint32_t received_checksum = buffer.ReadInteger();
        sequence = buffer.ReadShort();
        
        // compute checksum on all data not including checksum and serialization check
        uint32_t computed_checksum = crc32(
            buffer.GetRawData() + PACKET_DATA_BEGIN,
            packet_data_size
        );
        if (received_checksum != computed_checksum) {
            std::cout << "Checksum failed!\n";
            return PacketError::ChecksumError;
        }

        ReadData(buffer);

        uint32_t serialization_check = buffer.ReadInteger();
        if (serialization_check != SERIALIZATION_CHECK) {
            std::cout << "Serialization check failed!\n";
            return PacketError::SerializationError;
        }

        if (!buffer.IsAtEnd()) {
            std::cerr << "Packet size does not match expected packet size!\n";
            return PacketError::SizeError;
        }

        return PacketError::NoError;
    }
#undef  REGISTER_PACKET

    // Write to buffer using this structure
    inline void Write(NetworkBuffer &buffer) const {
        NetworkBuffer packet_data_buffer;
        WriteData(packet_data_buffer);

        uint32_t checksum = crc32(packet_data_buffer.GetRawData(), packet_data_buffer.GetSize());
        
        buffer.Write(checksum);
        buffer.WriteShort(sequence);
        buffer.Insert(packet_data_buffer);
        buffer.Write(SERIALIZATION_CHECK);
    }

private:
    inline void ReadData(NetworkBuffer &buffer) {
        packet_type = static_cast<PacketType>(buffer.ReadByte());
        // if (packet_type == PacketType::Fragment) {
        //     fragment_header.Read(buffer);
        //     Assert(fragment_header.fragment_id < fragment_header.fragment_count, "Network packet fragment index out-of-bounds!");
        // }

        switch (packet_type) {
#define REGISTER_PACKET(pack)                                     \
            case PacketType::pack: {                              \
                auto &data = packet_data.emplace<Packet##pack>(); \
                data.Read(buffer);                                \
                break;                                            \
            }
            
            REGISTER_PACKET(Fragment)
            REGISTER_PACKET(Slice)
            REGISTER_PACKET(SliceAck)
            REGISTER_PACKET(ConnectionResponse)
            REGISTER_PACKET(Heartbeat)
            REGISTER_PACKET(ClientLeave)
            REGISTER_PACKET(MovePlayer)
            REGISTER_PACKET(ChangeView)
            REGISTER_PACKET(JoinResult)
            REGISTER_PACKET(PlayerState)
            REGISTER_PACKET(Test)
            REGISTER_PACKET(LargeTest)

            case PacketType::ClientJoin:
            case PacketType::Invalid:
            default:
                packet_data = std::monostate{};
                break;
        }
    }
#undef REGISTER_PACKET

    // Write only the data into this buffer, ignoring checksum and serialization
    // check
    inline void WriteData(NetworkBuffer &buffer) const {
        PacketType packet_section_type = packet_type;
        buffer.Write(static_cast<uint8_t>(packet_type));
        // if (packet_type == PacketType::Fragment) {
        //     fragment_header.Write(buffer);
        //     packet_section_type = fragment_header.packet_type;
        // }

        switch (packet_section_type) {
#define REGISTER_PACKET(pack)                                                \
            case PacketType::pack: {                                         \
                const auto *data = std::get_if<Packet##pack>(&packet_data);  \
                Assert(data != nullptr, "Packet type mismatch");             \
                data->Write(buffer);                                         \
                break;                                                       \
            }
            
            REGISTER_PACKET(Fragment)
            REGISTER_PACKET(Slice)
            REGISTER_PACKET(SliceAck)
            REGISTER_PACKET(ConnectionResponse)
            REGISTER_PACKET(Heartbeat)
            REGISTER_PACKET(ClientLeave)
            REGISTER_PACKET(MovePlayer)
            REGISTER_PACKET(ChangeView)
            REGISTER_PACKET(JoinResult)
            REGISTER_PACKET(PlayerState)
            REGISTER_PACKET(Test)
            REGISTER_PACKET(LargeTest)

            case PacketType::ClientJoin:
            case PacketType::Invalid:
            default:
                break;
        }
    }
#undef  REGISTER_PACKET
};

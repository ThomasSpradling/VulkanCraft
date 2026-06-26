#pragma once

#include "../Platform/Sockets/NetworkBuffer.h"

#include "../Utils/NonCopyable.h"

enum class PacketType : uint8_t {
    Invalid = 0,
    // System
    Fragment,
    Slice,
    SliceAck,
    ConnectionRequest,
    DisconnectRequest,
    ConnectionResponse,
    DisconnectResponse,

    // Other
    Test,
    LargeTest,
};

enum class PacketError : uint8_t {
    NoError = 0,
    SerializationError,
    SizeError,
    ChecksumError,
};

/**
 * Basic Packet Structure:
 * 0    CHECKSUM            (4 bytes)
 * 4    CHANNEL_ID          (1 byte)
 * 5    SEQUENCE            (2 bytes) 
 * 7    PACKET_TYPE         (1 byte)
 * --------------------------------------
 * 8    PACKET_DATA
 * ...
 * --------------------------------------
 * x    SERIALIZATION_CHECK (4 bytes)
 *
 */

const uint32_t PACKET_DATA_BEGIN = 8;   // Checksum + sequence number + packet type + channel
const uint32_t PACKET_DATA_END = 4;     // Serialization

class Packet : public NonCopyable {
public:
    uint8_t channel_id = 0;
    uint16_t sequence = 0;
public:
    virtual ~Packet() = default;

    // Read packet data into buffer
    NetworkBuffer Serialize() const;

    // Write buffer data into a packet
    static std::unique_ptr<Packet> Deserialize(NetworkBuffer &buffer);

    virtual PacketType Type() const = 0;

    uint32_t GetSize() const {
        return GetDataSize() + PACKET_DATA_BEGIN + PACKET_DATA_END;
    }
protected:
    uint32_t m_size = 0;
protected:
    static std::unique_ptr<Packet> CreateBlankPacket(PacketType type);

    virtual void SerializeData(NetworkBuffer &buffer) const {};
    virtual void DeserializeData(NetworkBuffer &buffer) {};
    virtual uint32_t GetDataSize() const { return 0; };
private:
};

#pragma once

#include <array>
#include <climits>
#include <cstdint>
#include "../../Platform/Sockets/NetworkBuffer.h"
#include "../../errors.h"
#include "../Packet.h"

constexpr uint32_t MAX_PACKET_FRAGMENT_SIZE = 1024;
constexpr uint32_t MAX_FRAGMENTS_PER_PACKET = 256;
constexpr uint32_t MAX_NETWORK_FRAGMENT_SIZE = MAX_PACKET_FRAGMENT_SIZE * MAX_FRAGMENTS_PER_PACKET;

constexpr uint32_t MAX_PACKET_SLICE_SIZE = 1024;
constexpr uint32_t MAX_SLICES_PER_CHUNK = 256;
constexpr uint32_t MAX_NETWORK_CHUNK_SIZE = MAX_PACKET_SLICE_SIZE * MAX_SLICES_PER_CHUNK;

constexpr uint32_t BITS_PER_BYTE = CHAR_BIT;

class ConnectionRequestPacket : public Packet {
public:
    virtual PacketType Type() const override { return PacketType::ConnectionRequest; }
};

class DisconnectRequestPacket : public Packet {
public:
    virtual PacketType Type() const override { return PacketType::DisconnectRequest; }
};

class DisconnectResponsePacket : public Packet {
public:
    virtual PacketType Type() const override { return PacketType::DisconnectResponse; }
};

class ConnectionResponsePacket : public Packet {
public:
    bool connection_accepted = false;
public:
    virtual void SerializeData(NetworkBuffer &buffer) const override {
        buffer.WriteBoolean(connection_accepted);
    }

    virtual void DeserializeData(NetworkBuffer &buffer) override {
        connection_accepted = buffer.ReadBoolean();
    }

    virtual PacketType Type() const override { return PacketType::ConnectionResponse; }
};

struct FragmentPacket : public Packet {
public:
    uint8_t fragment_id = 0;
    uint8_t num_fragments = 0;
    uint16_t fragment_bytes = 0;
    std::array<uint8_t, MAX_PACKET_FRAGMENT_SIZE> data {};  
public:
    virtual void SerializeData(NetworkBuffer &buffer) const override {
        Assert(fragment_id < num_fragments, "Fragment Packet Write: Fragment id out of bounds!");

        buffer.WriteByte(fragment_id);
        buffer.WriteByte(num_fragments);

        if (fragment_id == num_fragments - 1)
            buffer.WriteShort(fragment_bytes);
        else
            Assert(fragment_bytes == MAX_PACKET_FRAGMENT_SIZE, "Fragment Packet Write: A non-full packet fragment should only be possible on the final packet!");

        for (uint32_t i = 0; i < fragment_bytes; ++i) {
            buffer.WriteByte(data[i]);
        }
    }

    virtual void DeserializeData(NetworkBuffer &buffer) override {
        fragment_id = buffer.ReadByte();
        num_fragments = buffer.ReadByte();

        Assert(fragment_id < num_fragments, "Fragment Packet Read: Fragment id out of bounds!");

        fragment_bytes = (fragment_id == num_fragments - 1) ? buffer.ReadShort() : MAX_PACKET_FRAGMENT_SIZE;
        for (uint32_t i = 0; i < fragment_bytes; ++i) {
            data[i] = buffer.ReadByte();
        }
    }

    virtual PacketType Type() const override { return PacketType::Fragment; }
};

/**
 * Network Chunks are units of up to 256 KiB of data to be sent. This packet represents a 1 KiB slice of such a chunk
 * and it MUST be sent reliably using PacketSliceAcks, though not necessarily in-order.
 */
struct SlicePacket : public Packet {
public:
    uint16_t chunk_id = 0;      // ID of the Network Chunk being sent
    uint8_t slice_id = 0;       // Current slice
    uint8_t num_slices = 0;     // Out of number of slices in this chunk
    uint16_t slice_bytes = 0;   // Number of bytes in a slice; only sent over network for last slice
    std::array<uint8_t, MAX_PACKET_SLICE_SIZE> data {};
public:
    virtual void SerializeData(NetworkBuffer &buffer) const override {
        Assert(slice_id < num_slices, "Slice Packet Write: Slice id out of bounds!");

        buffer.WriteShort(chunk_id);
        buffer.WriteByte(slice_id);
        buffer.WriteByte(num_slices);

        if (slice_id == num_slices - 1)
            buffer.WriteShort(slice_bytes);
        else
            Assert(slice_bytes == MAX_PACKET_SLICE_SIZE, "Slice Packet Write: A non-full packet slice should only be possible on the final packet!");

        for (uint32_t i = 0; i < slice_bytes; ++i) {
            buffer.WriteByte(data[i]);
        }
    }

    virtual void DeserializeData(NetworkBuffer &buffer) override {
        chunk_id = buffer.ReadShort();
        slice_id = buffer.ReadByte();
        num_slices = buffer.ReadByte();

        Assert(slice_id < num_slices, "Slice Packet Read: Slice id out of bounds!");

        slice_bytes = (slice_id == num_slices - 1) ? buffer.ReadShort() : MAX_PACKET_SLICE_SIZE;
        for (uint32_t i = 0; i < slice_bytes; ++i) {
            data[i] = buffer.ReadByte();
        }
    }

    virtual PacketType Type() const override { return PacketType::Slice; }
};

struct SliceAckPacket : public Packet {
public:
    uint16_t chunk_id = 0;
    uint8_t num_slices = 0;
    std::array<bool, MAX_SLICES_PER_CHUNK> acks {};
public:
    virtual void SerializeData(NetworkBuffer &buffer) const override {
        buffer.WriteShort(chunk_id);
        buffer.WriteByte(num_slices);

        for (uint32_t i = 0; i < num_slices; i += BITS_PER_BYTE) {
            uint8_t byte = 0;

            uint32_t remaining_bits = std::min(BITS_PER_BYTE, num_slices - i);
            for (uint32_t bit = 0; bit < remaining_bits; ++bit) {
                byte |= static_cast<uint8_t>(acks[i + bit]) << bit;
            }

            buffer.WriteByte(byte);
        }
    }

    virtual void DeserializeData(NetworkBuffer &buffer) override {
        chunk_id = buffer.ReadShort();
        num_slices = buffer.ReadByte();

        for (uint32_t i = 0; i < num_slices; i += BITS_PER_BYTE) {
            uint8_t byte = buffer.ReadByte();
            
            uint32_t remaining_bits = std::min(BITS_PER_BYTE, num_slices - i);
            for (uint32_t bit = 0; bit < remaining_bits; ++bit) {
                acks[i + bit] = static_cast<bool>((byte >> bit) & 1);
            }
        }
    }

    virtual PacketType Type() const override { return PacketType::SliceAck; }
};

struct TestPacket : public Packet {
public:
    uint32_t value = 0;
public:
    virtual void SerializeData(NetworkBuffer &buffer) const override {
        buffer.Write(value);
    }

    virtual void DeserializeData(NetworkBuffer &buffer) override {
        value = buffer.ReadInteger();
    }

    virtual PacketType Type() const override { return PacketType::Test; }
};

constexpr uint32_t MAX_SIZE = 4096 * 4;
struct LargeTestPacket : public Packet {
public:
    uint32_t num_items = 0;
    std::vector<uint32_t> items {};
public:
    virtual void SerializeData(NetworkBuffer &buffer) const override {
        buffer.WriteInteger(num_items);
        for (uint32_t i = 0; i < num_items; ++i) {
            buffer.Write(items[i]);
        }
    }

    virtual void DeserializeData(NetworkBuffer &buffer) override {
        num_items = buffer.ReadInteger();
        items.reserve(num_items);
        for (uint32_t i = 0; i < num_items; ++i) {
            items[i] = buffer.ReadInteger();
        }
    }

    virtual PacketType Type() const override { return PacketType::LargeTest; }
};

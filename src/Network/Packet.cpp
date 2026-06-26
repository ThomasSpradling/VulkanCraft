#include "Packet.h"
#include <crc32.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include "Packets/SystemPackets.h"
#include "Protocol.h"

NetworkBuffer Packet::Serialize() const {
    NetworkBuffer packet_data_buffer;
    packet_data_buffer.WriteByte(static_cast<uint8_t>(Type()));
    SerializeData(packet_data_buffer);

    uint32_t checksum = crc32(packet_data_buffer.GetRawData(), packet_data_buffer.GetSize());
    
    NetworkBuffer buffer;
    buffer.Write(checksum);
    buffer.WriteByte(channel_id);
    buffer.WriteShort(sequence);
    buffer.Insert(packet_data_buffer);
    buffer.Write(SERIALIZATION_CHECK);
    return buffer;
}

std::unique_ptr<Packet> Packet::Deserialize(NetworkBuffer &buffer) {
    Assert(buffer.GetSize() >= PACKET_DATA_BEGIN + PACKET_DATA_END, "Invalid packet!");
    uint32_t packet_data_size = buffer.GetSize() - PACKET_DATA_BEGIN - PACKET_DATA_END + 1;
    uint32_t received_checksum = buffer.ReadInteger();
    uint8_t channel = buffer.ReadByte();
    uint16_t sequence = buffer.ReadShort();
    
    // compute checksum on all data not including checksum and serialization check
    uint32_t computed_checksum = crc32(
        buffer.GetRawData() + PACKET_DATA_BEGIN - 1, // include the packet type
        packet_data_size
    );

    if (received_checksum != computed_checksum) {
        std::cerr << "Checksum failed!\n";
        return nullptr;
    }

    PacketType packet_type = static_cast<PacketType>(buffer.ReadByte());
    std::unique_ptr<Packet> packet = Packet::CreateBlankPacket(packet_type);

    packet->channel_id = channel;
    packet->sequence = sequence;
    packet->DeserializeData(buffer);

    uint32_t serialization_check = buffer.ReadInteger();
    if (serialization_check != SERIALIZATION_CHECK) {
        std::cerr << "Serialization check failed!\n";
        return nullptr;
    }

    if (!buffer.IsAtEnd()) {
        std::cerr << "Packet size does not match expected packet size!\n";
        return nullptr;
    }

    return packet;
}

std::unique_ptr<Packet> Packet::CreateBlankPacket(PacketType type) {
    switch (type) {
#define REGISTER_PACKET(name)                           \
        case PacketType::name:                          \
            return std::make_unique<name##Packet>();

        REGISTER_PACKET(Fragment)
        REGISTER_PACKET(Slice)
        REGISTER_PACKET(SliceAck)
        REGISTER_PACKET(ConnectionRequest)
        REGISTER_PACKET(ConnectionResponse)
        REGISTER_PACKET(DisconnectRequest)
        REGISTER_PACKET(DisconnectResponse)
        REGISTER_PACKET(Test)
        REGISTER_PACKET(LargeTest)

#undef REGISTER_PACKET
        default:
            throw std::runtime_error("Requested invalid packet!");
    }
}

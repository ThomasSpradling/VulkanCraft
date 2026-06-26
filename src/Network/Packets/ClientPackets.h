#pragma once

#include <array>
#include <cstdint>
#include "../../Platform/Sockets/NetworkBuffer.h"
#include "../../errors.h"
#include "../Packet.h"

class HeartbeatPacket : public Packet {
public:
    uint32_t player_id;
public:
    virtual void SerializeData(NetworkBuffer &buffer) const override {
        buffer.Write(player_id);
    }

    virtual void DeserializeData(NetworkBuffer &buffer) override {
        player_id = buffer.ReadInteger();
    }
};

class ClientLeavePacket : public Packet {
public:
    uint32_t player_id;
public:
    virtual void SerializeData(NetworkBuffer &buffer) const override {
        buffer.Write(player_id);
    }

    virtual void DeserializeData(NetworkBuffer &buffer) override {
        player_id = buffer.ReadInteger();
    }
};

class MovePlayerPacket : public Packet {
public:
    uint32_t player_id;
    glm::vec3 direction;
public:
    virtual void SerializeData(NetworkBuffer &buffer) const override {
        buffer.Write(player_id);
        buffer.Write(direction);
    }

    virtual void DeserializeData(NetworkBuffer &buffer) override {
        player_id = buffer.ReadInteger();
        direction = buffer.ReadVec3();
    }
};

class ChangeViewPacket : public Packet {
public:
    uint32_t player_id;
    float yaw;
    float pitch;
public:
    virtual void SerializeData(NetworkBuffer &buffer) const override {
        buffer.Write(player_id);
        buffer.Write(yaw);
        buffer.Write(pitch);
    }

    virtual void DeserializeData(NetworkBuffer &buffer) override {
        player_id = buffer.ReadInteger();
        yaw = buffer.ReadFloat();
        pitch = buffer.ReadFloat();
    }
};

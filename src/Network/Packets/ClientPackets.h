#pragma once

#include <array>
#include <cstdint>
#include "../../Platform/Sockets/NetworkBuffer.h"
#include "../../errors.h"

struct PacketTest {
    uint32_t value;

    inline void Read(NetworkBuffer &buffer) {
        value = buffer.ReadInteger();
    }

    inline void Write(NetworkBuffer &buffer) const {
        buffer.Write(value);
    }
};

constexpr uint32_t MAX_SIZE = 4096 * 4;
struct PacketLargeTest {
    uint32_t num_items = 0;
    std::vector<uint32_t> items {};

    inline void Read(NetworkBuffer &buffer) {
        num_items = buffer.ReadInteger();
        items.reserve(num_items);
        for (uint32_t i = 0; i < num_items; ++i) {
            items[i] = buffer.ReadInteger();
        }
    }

    inline void Write(NetworkBuffer &buffer) const {
        buffer.WriteInteger(num_items);
        for (uint32_t i = 0; i < num_items; ++i) {
            buffer.Write(items[i]);
        }
    }
};

struct PacketHeartbeat {
    uint32_t player_id;

    inline void Read(NetworkBuffer &buffer) {
        player_id = buffer.ReadInteger();
    }

    inline void Write(NetworkBuffer &buffer) const {
        buffer.Write(player_id);
    }
};

struct PacketClientLeave {
    uint32_t player_id;

    inline void Read(NetworkBuffer &buffer) {
        player_id = buffer.ReadInteger();
    }

    inline void Write(NetworkBuffer &buffer) const {
        buffer.Write(player_id);
    }
};

struct PacketMovePlayer {
    uint32_t player_id;
    glm::vec3 direction;

    inline void Read(NetworkBuffer &buffer) {
        player_id = buffer.ReadInteger();
        direction = buffer.ReadVec3();
    }

    inline void Write(NetworkBuffer &buffer) const {
        buffer.Write(player_id);
        buffer.Write(direction);
    }
};

struct PacketChangeView {
    uint32_t player_id;
    float yaw;
    float pitch;

    inline void Read(NetworkBuffer &buffer) {
        player_id = buffer.ReadInteger();
        yaw = buffer.ReadFloat();
        pitch = buffer.ReadFloat();
    }

    inline void Write(NetworkBuffer &buffer) const {
        buffer.Write(player_id);
        buffer.Write(yaw);
        buffer.Write(pitch);
    }
};

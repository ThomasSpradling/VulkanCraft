#pragma once

#include <cstdint>
#include "../NetworkBuffer.h"

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
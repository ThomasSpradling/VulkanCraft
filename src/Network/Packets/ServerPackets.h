#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "../../errors.h"
#include "../NetworkBuffer.h"

struct PacketJoinResult {
    bool is_accepted;
    uint32_t player_id = 0;

    inline void Read(NetworkBuffer &buffer) {
        is_accepted = buffer.ReadBoolean();
        player_id = buffer.ReadInteger();
    }

    inline void Write(NetworkBuffer &buffer) const {
        buffer.Write(is_accepted);
        buffer.Write(player_id);
    }
};

/**
 * Packet Structure:
 * 0    PLAYER_COUNT (4 bytes)
 * 4    ID_0  (4 bytes)     POSITION_0 (12 bytes)
 * 20   ID_1  (4 bytes)     POSITION_1 (12 bytes)
 * ...
 */
struct PacketPlayerState {
    uint32_t count;
    std::vector<uint32_t> ids;
    std::vector<glm::vec3> positions;

    inline void Read(NetworkBuffer &buffer) {
        count = buffer.ReadInteger();
        ids.resize(count);
        positions.resize(count);

        for (uint32_t i = 0; i < count; ++i) {
            ids[i] = buffer.ReadInteger();
            positions[i] = buffer.ReadVec3();
        }
    }

    inline void Write(NetworkBuffer &buffer) const {
        buffer.Write(count);
        Assert(positions.size() == count, "Size mismatch in writing server packet PacketState");

        for (uint32_t i = 0; i < count; ++i) {
            buffer.Write(ids[i]);
            buffer.Write(positions[i]);
        }
    }
};

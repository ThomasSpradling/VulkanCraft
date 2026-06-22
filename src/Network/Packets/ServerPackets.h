#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "../../errors.h"
#include "../../Platform/Sockets/NetworkBuffer.h"

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
 * 4    ID_0  (4 bytes)     POSITION_0 (12 bytes)     DIRECTION_0 (8 bytes)
 * 28   ID_1  (4 bytes)     POSITION_1 (12 bytes)     DIRECTION_1 (8 bytes)
 * ...
 */
struct PacketPlayerState {
    struct Data {
        uint32_t id;
        glm::vec3 position;
        float yaw, pitch;
    };
    
    uint32_t count;
    std::vector<Data> data;

    inline void Read(NetworkBuffer &buffer) {
        count = buffer.ReadInteger();
        data.resize(count);

        for (uint32_t i = 0; i < count; ++i) {
            data[i].id = buffer.ReadInteger();
            data[i].position = buffer.ReadVec3();
            data[i].yaw = buffer.ReadFloat();
            data[i].pitch = buffer.ReadFloat();
        }
    }

    inline void Write(NetworkBuffer &buffer) const {
        buffer.Write(count);
        Assert(data.size() == count, "Size mismatch in writing server packet PacketState");

        for (uint32_t i = 0; i < count; ++i) {
            buffer.Write(data[i].id);
            buffer.Write(data[i].position);
            buffer.Write(data[i].yaw);
            buffer.Write(data[i].pitch);
        }
    }
};

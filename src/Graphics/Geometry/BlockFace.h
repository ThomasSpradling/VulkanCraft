#pragma once

#include <array>
#include <glm/glm.hpp>

enum class BlockFaceType {
    Right,  // +X
    Left,   // -X
    Top,    // +Y
    Bottom, // -Y
    Front,  // +Z
    Back,   // -Z
};

struct BlockFace {
    BlockFaceType type;
    std::array<glm::vec3, 4> positions;
    glm::vec3 normal;
};

constexpr BlockFace BLOCK_FACE_LEFT {
    .type = BlockFaceType::Left,
    .positions = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
    },
    .normal = glm::vec3(-1.0f, 0.0f, 0.0f),
};

constexpr BlockFace BLOCK_FACE_RIGHT {
    .type = BlockFaceType::Right,
    .positions = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
    },
    .normal = glm::vec3(1.0f, 0.0f, 0.0f),
};

constexpr BlockFace BLOCK_FACE_BOTTOM {
    .type = BlockFaceType::Bottom,
    .positions = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
    },
    .normal = glm::vec3(0.0f, -1.0f, 0.0f),
};

constexpr BlockFace BLOCK_FACE_TOP {
    .type = BlockFaceType::Top,
    .positions = {
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 1.0f),
    },
    .normal = glm::vec3(0.0f, 1.0f, 0.0f),
};

constexpr BlockFace BLOCK_FACE_BACK {
    .type = BlockFaceType::Back,
    .positions = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
    },
    .normal = glm::vec3(0.0f, 0.0f, -1.0f),
};

constexpr BlockFace BLOCK_FACE_FRONT {
    .type = BlockFaceType::Front,
    .positions = {
        glm::vec3(1.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
    },
    .normal = glm::vec3(0.0f, 0.0f, 1.0f),
};

constexpr std::array<BlockFace, 6> BLOCK_FACE_LIST {
    BLOCK_FACE_FRONT,
    BLOCK_FACE_BACK,
    BLOCK_FACE_RIGHT,
    BLOCK_FACE_LEFT,
    BLOCK_FACE_TOP,
    BLOCK_FACE_BOTTOM,
};

inline constexpr const BlockFace &GetBlockFace(BlockFaceType face) {
    switch (face) {
        case BlockFaceType::Right:
            return BLOCK_FACE_RIGHT;
        case BlockFaceType::Left:
            return BLOCK_FACE_LEFT;
        case BlockFaceType::Top:
            return BLOCK_FACE_TOP;
        case BlockFaceType::Bottom:
            return BLOCK_FACE_BOTTOM;
        case BlockFaceType::Front:
            return BLOCK_FACE_FRONT;
        case BlockFaceType::Back:
            return BLOCK_FACE_BACK;
    }
}

// This file was automatically generated. Do not edit manually.

#include "../Graphics/Geometry/BlockFace.h"

#include <array>
#include <cstdint>
#include <string_view>

enum class BlockType : uint16_t {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    Grass = 3,
    Water = 4,
    Wood = 5,
    Leaves = 6,
    Glass = 7,
    Snow = 8,
};

inline constexpr uint32_t BLOCK_TEXTURE_COUNT = 12;

inline constexpr std::array<std::string_view, BLOCK_TEXTURE_COUNT> BLOCK_TEXTURE_PATHS = {
    ASSET_PATH "/textures/blocks/dirt.png", // 0
    ASSET_PATH "/textures/blocks/glass.png", // 1
    ASSET_PATH "/textures/blocks/grass_plant.png", // 2
    ASSET_PATH "/textures/blocks/grass_side.png", // 3
    ASSET_PATH "/textures/blocks/grass_top.png", // 4
    ASSET_PATH "/textures/blocks/leaves.png", // 5
    ASSET_PATH "/textures/blocks/sand.png", // 6
    ASSET_PATH "/textures/blocks/snow.png", // 7
    ASSET_PATH "/textures/blocks/stone.png", // 8
    ASSET_PATH "/textures/blocks/water.png", // 9
    ASSET_PATH "/textures/blocks/wood_side.png", // 10
    ASSET_PATH "/textures/blocks/wood_top.png", // 11
};

constexpr bool IsOpaque(BlockType type) {
    switch (type) {
        case BlockType::Air:
            return false;

        case BlockType::Stone:
            return true;

        case BlockType::Dirt:
            return true;

        case BlockType::Grass:
            return true;

        case BlockType::Water:
            return false;

        case BlockType::Wood:
            return true;

        case BlockType::Leaves:
            return false;

        case BlockType::Glass:
            return false;

        case BlockType::Snow:
            return true;
    }

    return false;
}

constexpr uint32_t GetTextureID(BlockType block_type, BlockFaceType block_face) {
    switch (block_type) {
        case BlockType::Air:
            return 0;

        case BlockType::Stone:
            return 8;

        case BlockType::Dirt:
            return 0;

        case BlockType::Grass:
            switch (block_face) {
                case BlockFaceType::Right:
            return 3;

        case BlockFaceType::Left:
            return 3;

        case BlockFaceType::Top:
            return 4;

        case BlockFaceType::Bottom:
            return 0;

        case BlockFaceType::Front:
            return 3;

        case BlockFaceType::Back:
            return 3;
            }

            return 0;

        case BlockType::Water:
            return 9;

        case BlockType::Wood:
            switch (block_face) {
                case BlockFaceType::Right:
            return 10;

        case BlockFaceType::Left:
            return 10;

        case BlockFaceType::Top:
            return 11;

        case BlockFaceType::Bottom:
            return 11;

        case BlockFaceType::Front:
            return 10;

        case BlockFaceType::Back:
            return 10;
            }

            return 0;

        case BlockType::Leaves:
            return 5;

        case BlockType::Glass:
            return 1;

        case BlockType::Snow:
            return 7;
    }

    return 0;
}

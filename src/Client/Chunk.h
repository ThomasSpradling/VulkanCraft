#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include "../Graphics/VulkanRenderer.h"
#include "../Graphics/gpu_structs.h"

#include "BlockData.h"

struct alignas(16) BlockVertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    int texture_id = 0;
    int32_t pad0;
    int32_t pad1;
    int32_t pad2;
};

struct Block {
    BlockType block_type = BlockType::Air;
};

class Chunk {
public:
    Chunk(const VulkanRenderer &renderer) : m_renderer(renderer) {
        for (uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
            for (uint32_t z = 0; z < CHUNK_LENGTH; ++z) {
                int cx = static_cast<int>(CHUNK_WIDTH) / 2;
                int cz = static_cast<int>(CHUNK_LENGTH) / 2;

                int dx = static_cast<int>(x) - cx;
                int dz = static_cast<int>(z) - cz;

                float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz));

                int max_y = static_cast<int>(10.0f - dist * 0.45f);
                max_y += static_cast<int>(2.0f * std::sin(static_cast<float>(x) * 0.5f));
                max_y += static_cast<int>(1.5f * std::cos(static_cast<float>(z) * 0.6f));

                max_y = std::clamp(max_y, 1, static_cast<int>(CHUNK_HEIGHT) - 1);

                for (uint32_t y = 0; y <= static_cast<uint32_t>(max_y); ++y) {
                    if (y == static_cast<uint32_t>(max_y)) {
                        PlaceBlock(x, y, z, BlockType::Grass);
                    } else if (y + 3 >= static_cast<uint32_t>(max_y)) {
                        PlaceBlock(x, y, z, BlockType::Dirt);
                    } else {
                        PlaceBlock(x, y, z, BlockType::Stone);
                    }
                }
            }
        }
    }

    void PlaceBlock(uint32_t x, uint32_t y, uint32_t z, BlockType type);
    void PlaceBlock(glm::ivec3 position, BlockType type) { PlaceBlock(position.x, position.y, position.z, type); }

    const Block &GetBlock(uint32_t x, uint32_t y, uint32_t z) const;
    const Block &GetBlock(glm::ivec3 position) { return GetBlock(position.x, position.y, position.z); }

    GPUMesh CreateMesh() const;
private:
    static constexpr uint32_t CHUNK_LENGTH = 16;
    static constexpr uint32_t CHUNK_WIDTH  = 16;
    static constexpr uint32_t CHUNK_HEIGHT = 256;

    std::array<std::array<std::array<Block, CHUNK_WIDTH>, CHUNK_HEIGHT>, CHUNK_LENGTH> m_blocks;
    const VulkanRenderer &m_renderer;
};

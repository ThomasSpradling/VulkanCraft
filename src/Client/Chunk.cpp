#include "Chunk.h"
#include <vector>
#include "../Graphics/Geometry/BlockFace.h"

void Chunk::PlaceBlock(uint32_t x, uint32_t y, uint32_t z, BlockType type) {
    m_blocks[x][y][z] = Block{
        .block_type = type
    };
}

const Block &Chunk::GetBlock(uint32_t x, uint32_t y, uint32_t z) const {
    return m_blocks[x][y][z];
}

GPUMesh Chunk::CreateMesh() const {
    std::vector<BlockVertex> vertices;
    std::vector<uint32_t> indices;

    uint32_t index = 0;
    for (uint32_t x = 0; x < CHUNK_WIDTH; ++x) {
        for (uint32_t z = 0; z < CHUNK_LENGTH; ++z) {
            for (uint32_t y = 0; y < CHUNK_HEIGHT; ++y) {
                const Block &block = GetBlock(x, y, z);
                if (block.block_type == BlockType::Air)
                    continue;

                glm::vec3 position = glm::vec3(x, y, z);
                for (const auto &face : BLOCK_FACE_LIST) {
                    int texture_id = GetTextureID(block.block_type, face.type);
                    vertices.insert(vertices.end(), {
                        { face.positions[0] + position, 0.0f, face.normal, 0.0f, texture_id },
                        { face.positions[1] + position, 1.0f, face.normal, 0.0f, texture_id },
                        { face.positions[2] + position, 1.0f, face.normal, 1.0f, texture_id },
                        { face.positions[3] + position, 0.0f, face.normal, 1.0f, texture_id },
                    });
                    indices.insert(indices.end(), {
                        index + 0, index + 1, index + 2,
                        index + 2, index + 3, index + 0
                    });
                    index += 4;
                }
            }
        }
    }
    
    return m_renderer.UploadGPUMesh(vertices, indices);
}

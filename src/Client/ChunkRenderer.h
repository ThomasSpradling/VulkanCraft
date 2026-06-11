#pragma once

#include "../Graphics/VulkanRenderer.h"
#include <vector>

struct BlockVertex {
    // TODO: Encode
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    // uint32_t block_id;
};

/**
 * @brief Handles the rendering of all chunks in the world.
 */
class ChunkRenderer {
public:
    ChunkRenderer(const VulkanRenderer &renderer) : m_renderer(renderer) {};
    void Init(VkDescriptorSetLayout global_layout);
    void Destroy();

    void LoadChunkMesh();
    void FreeChunkMesh();

    void Draw(VkCommandBuffer cmd, VkDescriptorSet global_descriptor_set);
private:
    struct PushConstantData {
        glm::mat4 model_transform;
        VkDeviceAddress vertex_buffer;
    };

    struct ChunkRenderData {
        uint32_t index_count;
        uint32_t start_index;
        glm::mat4 transform;

        GPUMesh mesh;
    };
private:
    const VulkanRenderer &m_renderer;

    PushConstantData m_push_constants;

    VkPipeline m_opaque_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_opaque_pipeline_layout = VK_NULL_HANDLE;
    std::vector<ChunkRenderData> m_chunk_data;
};

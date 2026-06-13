#pragma once

#include "../Graphics/VulkanRenderer.h"
#include "Chunk.h"
#include <vector>

/**
 * @brief Handles the rendering of all chunks in the world.
 */
class ChunkRenderer {
public:
    ChunkRenderer(const VulkanRenderer &renderer) : m_renderer(renderer) {};
    void Init(VkDescriptorSetLayout global_layout);
    void Destroy();

    void LoadChunkMesh(const Chunk &chunk, glm::ivec2 chunk_offset);
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

    std::unique_ptr<DescriptorAllocator> m_descriptor_allocator;
    VkDescriptorSetLayout m_block_texture_layout = VK_NULL_HANDLE;
    VkDescriptorSet m_block_texture_descriptor_set = VK_NULL_HANDLE;
    VkSampler m_block_texture_sampler = VK_NULL_HANDLE;

    GPUImage m_block_texture_array;

    PushConstantData m_push_constants;

    VkPipeline m_opaque_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_opaque_pipeline_layout = VK_NULL_HANDLE;
    std::vector<ChunkRenderData> m_chunk_data;
private:
    void InitializeTextures();
};

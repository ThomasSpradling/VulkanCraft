#include "ChunkRenderer.h"
#include "../Graphics/DescriptorLayoutBuilder.h"
#include "../Graphics/PipelineBuilder.h"
#include "../Graphics/DescriptorWriter.h"
#include "Chunk.h"
#include <glm/ext/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>

void ChunkRenderer::Draw(VkCommandBuffer cmd, VkDescriptorSet global_descriptor_set) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_opaque_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_opaque_pipeline_layout, 0, 1, &global_descriptor_set, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_opaque_pipeline_layout, 1, 1, &m_block_texture_descriptor_set, 0, nullptr);

    for (const auto &chunk_data : m_chunk_data) {
        // Bind chunk-level descriptor set

        vkCmdBindIndexBuffer(cmd, chunk_data.mesh.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        m_push_constants.vertex_buffer = chunk_data.mesh.device_address;
        m_push_constants.model_transform = chunk_data.transform;
        vkCmdPushConstants(cmd, m_opaque_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &m_push_constants);

        vkCmdDrawIndexed(cmd, chunk_data.index_count, 1, chunk_data.start_index, 0, 0);
    }
}

void ChunkRenderer::LoadChunkMesh(const Chunk &chunk, glm::ivec2 chunk_offset) {
    glm::mat4 translate = glm::translate(glm::mat4(1.0f), glm::vec3(chunk_offset.x * 16, 0.0, chunk_offset.y * 16));

    ChunkRenderData chunk_render_data;
    chunk_render_data.mesh = chunk.CreateMesh();
    chunk_render_data.index_count = chunk_render_data.mesh.index_count;
    chunk_render_data.start_index = 0;
    chunk_render_data.transform = translate;

    m_chunk_data.push_back(chunk_render_data);
}

void ChunkRenderer::FreeChunkMesh() {
    for (auto &chunk_data : m_chunk_data) {
        m_renderer.DestroyGPUMesh(chunk_data.mesh);
    }
}

void ChunkRenderer::Init(VkDescriptorSetLayout global_layout) {
    m_descriptor_allocator = std::make_unique<DescriptorAllocator>(m_renderer.GetDevice());
    m_descriptor_allocator->Init(10, {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
    });

    m_block_texture_layout = DescriptorLayoutBuilder(m_renderer.GetDevice())
        .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        .Build(VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPushConstantRange push_constant_data {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstantData),
    };
    VkShaderModule vertex_shader_module = m_renderer.LoadShader(ASSET_PATH "/shaders/chunks/block.vert.spv");
    VkShaderModule fragment_shader_module = m_renderer.LoadShader(ASSET_PATH "/shaders/chunks/block.frag.spv");

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts {
        global_layout,
        m_block_texture_layout // READ ONLY
    };
    std::vector<VkPushConstantRange> push_constant_ranges {
        push_constant_data
    };
    VkPipelineLayoutCreateInfo pipeline_layout_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size()),
        .pPushConstantRanges = push_constant_ranges.data(),
    };
    VK_CHECK(vkCreatePipelineLayout(m_renderer.GetDevice(), &pipeline_layout_create_info, nullptr, &m_opaque_pipeline_layout));

    m_opaque_pipeline = PipelineBuilder_Graphics(m_renderer.GetDevice(), m_opaque_pipeline_layout)
        .AddColorAttachmentFormat(m_renderer.GetColorFormat())
        .SetDepthAttachmentFormat(m_renderer.GetDepthOnlyFormat())
        // .SetPolygonMode(VK_POLYGON_MODE_LINE)
        .EnableDepthTest()
        .AddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader_module)
        .AddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader_module)
        .Build();

    m_block_texture_descriptor_set = m_descriptor_allocator->AllocateDescriptorSet(m_block_texture_layout);
    
    InitializeTextures();

    // TODO: Remove
    Chunk chunk(m_renderer);
    for (int x = -3; x < 3; ++x) {
        for (int z = -3; z < 3; ++z) {
            LoadChunkMesh(chunk, glm::ivec2(x, z));
        }
    }
    std::cout << "Loaded chunks!\n";
}

void ChunkRenderer::Destroy() { 
    FreeChunkMesh();

    vkDestroyImageView(m_renderer.GetDevice(), m_block_texture_array.image_view, nullptr);
    vmaDestroyImage(m_renderer.GetMemoryAllocator(), m_block_texture_array.image, m_block_texture_array.allocation);
    vkDestroySampler(m_renderer.GetDevice(), m_block_texture_sampler, nullptr);
    
    vkDestroyDescriptorSetLayout(m_renderer.GetDevice(), m_block_texture_layout, nullptr);
    vkDestroyPipelineLayout(m_renderer.GetDevice(), m_opaque_pipeline_layout, nullptr);
    vkDestroyPipeline(m_renderer.GetDevice(), m_opaque_pipeline, nullptr);

    m_descriptor_allocator->Destroy();
}

void ChunkRenderer::InitializeTextures() {
    std::vector<std::string> texture_paths(BLOCK_TEXTURE_PATHS.begin(), BLOCK_TEXTURE_PATHS.end());
    m_block_texture_array = m_renderer.LoadImageArray2D(texture_paths, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    std::cout << "Loaded images!\n";

    VkSamplerCreateInfo nearest_sampler_create_info {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
    };
    VK_CHECK(vkCreateSampler(m_renderer.GetDevice(), &nearest_sampler_create_info, nullptr, &m_block_texture_sampler));

    DescriptorWriter(m_renderer.GetDevice())
        .WriteImage(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {
            .sampler = m_block_texture_sampler,
            .imageView = m_block_texture_array.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        })
        .Write(m_block_texture_descriptor_set);
}

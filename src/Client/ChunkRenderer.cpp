#include "ChunkRenderer.h"
#include "../Graphics/DescriptorLayoutBuilder.h"
#include "../Graphics/PipelineBuilder.h"
#include <vulkan/vulkan_core.h>

void ChunkRenderer::Draw(VkCommandBuffer cmd, VkDescriptorSet global_descriptor_set) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_opaque_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_opaque_pipeline_layout, 0, 1, &global_descriptor_set, 0, nullptr);

    for (const auto &chunk_data : m_chunk_data) {
        // Bind chunk-level descriptor set

        vkCmdBindIndexBuffer(cmd, chunk_data.mesh.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        m_push_constants.vertex_buffer = chunk_data.mesh.device_address;
        m_push_constants.model_transform = glm::mat4(1.0f);
        vkCmdPushConstants(cmd, m_opaque_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &m_push_constants);

        vkCmdDrawIndexed(cmd, chunk_data.index_count, 1, chunk_data.start_index, 0, 1);
    }
}

void ChunkRenderer::LoadChunkMesh() {
    ChunkRenderData chunk_render_data;

    std::vector<BlockVertex> vertices = {
        // position                 uv_x  normal                  uv_y

        // Front face (+Z)
        {{-0.5f, -0.5f,  0.5f}, 0.0f, { 0.0f,  0.0f,  1.0f}, 0.0f},
        {{ 0.5f, -0.5f,  0.5f}, 1.0f, { 0.0f,  0.0f,  1.0f}, 0.0f},
        {{ 0.5f,  0.5f,  0.5f}, 1.0f, { 0.0f,  0.0f,  1.0f}, 1.0f},
        {{-0.5f,  0.5f,  0.5f}, 0.0f, { 0.0f,  0.0f,  1.0f}, 1.0f},

        // Back face (-Z)
        {{ 0.5f, -0.5f, -0.5f}, 0.0f, { 0.0f,  0.0f, -1.0f}, 0.0f},
        {{-0.5f, -0.5f, -0.5f}, 1.0f, { 0.0f,  0.0f, -1.0f}, 0.0f},
        {{-0.5f,  0.5f, -0.5f}, 1.0f, { 0.0f,  0.0f, -1.0f}, 1.0f},
        {{ 0.5f,  0.5f, -0.5f}, 0.0f, { 0.0f,  0.0f, -1.0f}, 1.0f},

        // Right face (+X)
        {{ 0.5f, -0.5f,  0.5f}, 0.0f, { 1.0f,  0.0f,  0.0f}, 0.0f},
        {{ 0.5f, -0.5f, -0.5f}, 1.0f, { 1.0f,  0.0f,  0.0f}, 0.0f},
        {{ 0.5f,  0.5f, -0.5f}, 1.0f, { 1.0f,  0.0f,  0.0f}, 1.0f},
        {{ 0.5f,  0.5f,  0.5f}, 0.0f, { 1.0f,  0.0f,  0.0f}, 1.0f},

        // Left face (-X)
        {{-0.5f, -0.5f, -0.5f}, 0.0f, {-1.0f,  0.0f,  0.0f}, 0.0f},
        {{-0.5f, -0.5f,  0.5f}, 1.0f, {-1.0f,  0.0f,  0.0f}, 0.0f},
        {{-0.5f,  0.5f,  0.5f}, 1.0f, {-1.0f,  0.0f,  0.0f}, 1.0f},
        {{-0.5f,  0.5f, -0.5f}, 0.0f, {-1.0f,  0.0f,  0.0f}, 1.0f},

        // Top face (+Y)
        {{-0.5f,  0.5f,  0.5f}, 0.0f, { 0.0f,  1.0f,  0.0f}, 0.0f},
        {{ 0.5f,  0.5f,  0.5f}, 1.0f, { 0.0f,  1.0f,  0.0f}, 0.0f},
        {{ 0.5f,  0.5f, -0.5f}, 1.0f, { 0.0f,  1.0f,  0.0f}, 1.0f},
        {{-0.5f,  0.5f, -0.5f}, 0.0f, { 0.0f,  1.0f,  0.0f}, 1.0f},

        // Bottom face (-Y)
        {{-0.5f, -0.5f, -0.5f}, 0.0f, { 0.0f, -1.0f,  0.0f}, 0.0f},
        {{ 0.5f, -0.5f, -0.5f}, 1.0f, { 0.0f, -1.0f,  0.0f}, 0.0f},
        {{ 0.5f, -0.5f,  0.5f}, 1.0f, { 0.0f, -1.0f,  0.0f}, 1.0f},
        {{-0.5f, -0.5f,  0.5f}, 0.0f, { 0.0f, -1.0f,  0.0f}, 1.0f},
    };

    std::vector<uint32_t> indices = {
        // Front
        0,  1,  2,
        2,  3,  0,

        // Back
        4,  5,  6,
        6,  7,  4,

        // Right
        8,  9, 10,
        10, 11,  8,

        // Left
        12, 13, 14,
        14, 15, 12,

        // Top
        16, 17, 18,
        18, 19, 16,

        // Bottom
        20, 21, 22,
        22, 23, 20,
    };
    
    chunk_render_data.mesh = m_renderer.UploadGPUMesh(vertices, indices);
    chunk_render_data.index_count = indices.size();
    chunk_render_data.start_index = 0;
    chunk_render_data.transform = glm::mat4(1.0f);

    m_chunk_data.push_back(chunk_render_data);
}

void ChunkRenderer::FreeChunkMesh() {
    m_renderer.DestroyGPUMesh(m_chunk_data[0].mesh);
}

void ChunkRenderer::Init(VkDescriptorSetLayout global_layout) {
    VkPushConstantRange push_constant_data {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstantData),
    };
    VkShaderModule vertex_shader_module = m_renderer.LoadShader(RESOURCE_PATH "/shaders/chunks/block.vert.spv");
    VkShaderModule fragment_shader_module = m_renderer.LoadShader(RESOURCE_PATH "/shaders/chunks/block.frag.spv");

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts {
        global_layout,
        // m_gltf_common_data.material_layout,
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
        .EnableDepthTest()
        .AddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader_module)
        .AddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader_module)
        .Build();

    // TODO: Remove
    LoadChunkMesh();
}

void ChunkRenderer::Destroy() {
    FreeChunkMesh();

    vkDestroyPipelineLayout(m_renderer.GetDevice(), m_opaque_pipeline_layout, nullptr);
    vkDestroyPipeline(m_renderer.GetDevice(), m_opaque_pipeline, nullptr);
}

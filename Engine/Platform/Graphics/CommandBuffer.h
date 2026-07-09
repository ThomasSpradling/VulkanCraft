#pragma once

#include "Platform/Graphics/Common.h"
#include <glm/glm.hpp>
#include <string_view>
#include <variant>
#include <vector>
#include <volk.h>

class VulkanImage;
class VulkanCommandPool;

class ImageBarrier {
public:
    ImageBarrier(VkCommandBuffer cmd, VulkanImage &image);

    // Selects a suitable access based on other parameters
    ImageBarrier &SourceAccess(MemoryAccessType access);
    ImageBarrier &DestAccess(MemoryAccessType access);
    
    ImageBarrier &SourceAccess(VkAccessFlags2 access);
    ImageBarrier &DestAccess(VkAccessFlags2 access);

    ImageBarrier &SourceStage(VkPipelineStageFlags2 stage);
    ImageBarrier &DestStage(VkPipelineStageFlags2 stage);

    ImageBarrier &TransitionLayout(VkImageLayout new_layout);
    ImageBarrier &SubresourceRange(const VkImageSubresourceRange &subresource);

    void Execute();
private:
    VulkanImage &m_image;
    VkCommandBuffer m_cmd;

    MemoryAccessType m_generated_src_access = MemoryAccessType::ReadWrite;
    MemoryAccessType m_generated_dst_access = MemoryAccessType::ReadWrite;

    VkAccessFlags2 m_src_access = 0;
    VkAccessFlags2 m_dst_access = 0;

    VkPipelineStageFlags2 m_src_stage = 0;
    VkPipelineStageFlags2 m_dst_stage = 0;

    VkImageLayout m_new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageSubresourceRange m_subresource {};
};


enum class AttachmentType : uint8_t {
    Color,
    Depth,
    Stencil
};

struct ImageAttachment {
    AttachmentType type;
    const VulkanImage &image;
    
    bool msaa = false;
    const VulkanImage *resolve_image = nullptr;

    bool should_clear = true;

    glm::vec4 clear_color = glm::vec4(0.0f);
    float clear_depth = 1.0f;
    uint32_t clear_stencil = 0u;
};

class CommandBuffer {
public:
    CommandBuffer(const VulkanCommandPool &command_pool);
    ~CommandBuffer();

    void Begin(VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) const;
    void End() const;

    VkCommandBuffer Handle() const { return m_command_buffer; }
    void SetDebugName(std::string_view name) const;
public:
    //// General Commands ////

    void BeginRendering(const VulkanImage &color_image, glm::vec4 clear_color = glm::vec4(0.0)) const;
    void BeginRendering(const VulkanImage &color_image, const VulkanImage &depth_image, glm::vec4 clear_color = glm::vec4(0.0), float clear_depth = 1.0f) const;
    void BeginRendering(const std::vector<ImageAttachment> &render_targets) const;
    void BeginRendering(const std::vector<ImageAttachment> &render_targets, VkRect2D render_area) const;
    void EndRendering() const;

    void BindGraphicsPipeline(VkPipeline pipeline) const;
    void BindComputePipeline(VkPipeline pipeline) const;
    void BindRayTracingPipeline(VkPipeline pipeline) const;

    void SetViewportAndScissor(glm::ivec2 offset, glm::uvec2 extent) const;

    void CopyImage(const VulkanImage &src, const VulkanImage &dst);

    void BindDescriptorSet(VkPipelineBindPoint bind_point, uint32_t set, VkPipelineLayout layout, VkDescriptorSet descriptor_set);
    
    template<typename T>
    void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stage, uint32_t offset, uint32_t size, const T &data) {
        vkCmdPushConstants(m_command_buffer, layout, stage, offset, size, &data);
    }

    template<typename T>
    void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stage, const T &data) {
        vkCmdPushConstants(m_command_buffer, layout, stage, 0, sizeof(T), &data);
    }

    //// Debug Commands ////
    void BeginLabel(const std::string &label, glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    void EndLabel();
    void InsertLabel(const std::string &label, glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    //// Graphics Commands ////

    void BindVertexBuffer(VkBuffer buffer, VkDeviceSize offset = 0);
    void BindIndexBuffer(VkBuffer buffer, VkDeviceSize offset = 0);

    void Draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0) const;
    void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0, uint32_t first_instance = 0) const;
public:
    //// Utilities ////

    ImageBarrier ImageMemoryBarrier(VulkanImage &image) const;
    void TransitionLayout(VulkanImage &image, VkImageLayout layout) const;
    void TransitionLayout(VulkanImage &image, VkImageLayout layout, const VkImageSubresourceRange &range) const;

    void GenerateMipMaps(VulkanImage &image, VkFilter filter, uint32_t layer = 0);
private:
    const VulkanCommandPool &m_command_pool;
    VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;
};

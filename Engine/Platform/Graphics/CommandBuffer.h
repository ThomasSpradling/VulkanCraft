#pragma once

#include "Platform/Graphics/Common.h"
#include <glm/glm.hpp>
#include <string_view>
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
    bool msaa = false;

    const VulkanImage &image;
    const VulkanImage *resolve_image = nullptr;
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
    void BeginRendering(const VulkanImage &color_image, const VulkanImage &depth_image, glm::vec4 clear_color = glm::vec4(0.0), float clear_depth = 0) const;
    void BeginRendering(const std::vector<ImageAttachment> &render_targets, glm::vec4 clear_color = glm::vec4(0.0), float clear_depth = 0, uint32_t clear_stencil = 0) const;
    void BeginRendering(const std::vector<ImageAttachment> &render_targets, VkRect2D render_area, glm::vec4 clear_color = glm::vec4(0.0), float clear_depth = 0, uint32_t clear_stencil = 0) const;
    void EndRendering() const;

    void BindGraphicsPipeline(VkPipeline pipeline) const;
    void BindComputePipeline(VkPipeline pipeline) const;
    void BindRayTracingPipeline(VkPipeline pipeline) const;

    void SetViewportAndScissor(glm::ivec2 offset, glm::uvec2 extent) const;

    void CopyImage(const VulkanImage &src, const VulkanImage &dst);

    //// Debug Commands ////
    void BeginLabel(const std::string &label, glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    void EndLabel();
    void InsertLabel(const std::string &label, glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    //// Graphics Commands ////

    void Draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0) const;
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

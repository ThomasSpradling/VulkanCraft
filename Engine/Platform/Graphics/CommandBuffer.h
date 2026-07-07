#pragma once

#include <string_view>
#include <volk.h>

class VulkanImage;
class VulkanCommandPool;

class CommandBuffer {
public:
    CommandBuffer(const VulkanCommandPool &command_pool);
    ~CommandBuffer();

    void Begin(VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) const;
    void End() const;

    void TransitionLayout(VulkanImage &image, VkImageLayout layout) const;

    void BeginRendering(VulkanImage &color_image, const VkClearValue &clear_value) const;
    void EndRendering() const;

    void BindGraphicsPipeline(VkPipeline pipeline) const;
    void SetViewportAndScissor(VkExtent2D extent) const;

    void Draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0) const;

    VkCommandBuffer Handle() const { return m_command_buffer; }
    void SetDebugName(std::string_view name) const;
private:
    const VulkanCommandPool &m_command_pool;
    VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;
};

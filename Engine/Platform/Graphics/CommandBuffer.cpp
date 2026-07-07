#include "CommandBuffer.h"
#include "VulkanObjects.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"

CommandBuffer::CommandBuffer(const VulkanCommandPool &command_pool)
    : m_command_pool(command_pool)
{
    VkCommandBufferAllocateInfo allocate_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_command_pool.m_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(vkAllocateCommandBuffers(m_command_pool.m_device.Device(), &allocate_info, &m_command_buffer));
}

CommandBuffer::~CommandBuffer() {
    vkFreeCommandBuffers(m_command_pool.m_device.Device(), m_command_pool.m_command_pool, 1, &m_command_buffer);
}

void CommandBuffer::Begin(VkCommandBufferUsageFlags flags) const {
    VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags,
    };
    VK_CHECK(vkBeginCommandBuffer(m_command_buffer, &begin_info));
}

void CommandBuffer::End() const {
    VK_CHECK(vkEndCommandBuffer(m_command_buffer));
}

void CommandBuffer::TransitionLayout(VulkanImage &image, VkImageLayout layout) const {
    image.TransitionLayout(m_command_buffer, layout);
}

void CommandBuffer::BeginRendering(VulkanImage &color_image, const VkClearValue &clear_value) const {
    const VkExtent3D extent = color_image.Extent();

    VkRenderingAttachmentInfo color_attachment {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = color_image.View(),
        .imageLayout = color_image.Layout(),
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear_value,
    };

    VkRenderingInfo rendering_info {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderArea = {
            .offset = { .x = 0, .y = 0 },
            .extent = {
                .width = extent.width,
                .height = extent.height,
            },
        },
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
        .pDepthAttachment = nullptr,
        .pStencilAttachment = nullptr,
    };

    vkCmdBeginRendering(m_command_buffer, &rendering_info);
}

void CommandBuffer::EndRendering() const {
    vkCmdEndRendering(m_command_buffer);
}

void CommandBuffer::BindGraphicsPipeline(VkPipeline pipeline) const {
    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void CommandBuffer::SetViewportAndScissor(VkExtent2D extent) const {
    VkViewport viewport {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor {
        .offset = { .x = 0, .y = 0 },
        .extent = extent,
    };

    vkCmdSetViewport(m_command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(m_command_buffer, 0, 1, &scissor);
}

void CommandBuffer::Draw(uint32_t vertex_count, uint32_t instance_count,uint32_t first_vertex, uint32_t first_instance) const {
    vkCmdDraw(m_command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void CommandBuffer::SetDebugName(std::string_view name) const {
    m_command_pool.m_device.SetDebugName(m_command_buffer, name);
}

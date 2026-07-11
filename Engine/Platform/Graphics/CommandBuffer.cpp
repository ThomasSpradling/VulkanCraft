#include "CommandBuffer.h"
#include "Common.h"
#include "VulkanObjects.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"
#include <stdexcept>

// ============================== //
// ---- Vulkan Image Builder ---- //
// ============================== //

ImageBarrier::ImageBarrier(VkCommandBuffer cmd, VulkanImage &image)
    : m_image(image)
    , m_cmd(cmd)
{
    m_new_layout = image.Layout();
    m_subresource = VkImageSubresourceRange{
        .baseMipLevel = 0,
        .levelCount = image.m_mip_levels,
        .baseArrayLayer = 0,
        .layerCount = image.m_array_layers,
    };
    m_subresource.aspectMask = GetFormatAspect(image.Format());   
}

// Selects a suitable access based on other parameters
ImageBarrier &ImageBarrier::SourceAccess(MemoryAccessType access) {
    m_generated_src_access = access;
    return *this;
}

ImageBarrier &ImageBarrier::DestAccess(MemoryAccessType access) {
    m_generated_dst_access = access;
    return *this;
}

ImageBarrier &ImageBarrier::SourceAccess(VkAccessFlags2 access) {
    m_src_access |= access;
    return *this;
}

ImageBarrier &ImageBarrier::DestAccess(VkAccessFlags2 access) {
    m_dst_access |= access;
    return *this;
}

ImageBarrier &ImageBarrier::SourceStage(VkPipelineStageFlags2 stage) {
    m_src_stage |= stage;
    return *this;
}

ImageBarrier &ImageBarrier::DestStage(VkPipelineStageFlags2 stage) {
    m_dst_stage |= stage;
    return *this;
}

ImageBarrier &ImageBarrier::TransitionLayout(VkImageLayout new_layout) {
    Assert(new_layout != VK_IMAGE_LAYOUT_UNDEFINED, "An image cannot be transitioned into LAYOUT_UNDEFINED!");

    m_new_layout = new_layout;
    return *this;
}

ImageBarrier &ImageBarrier::SubresourceRange(const VkImageSubresourceRange &subresource) {
    m_subresource = subresource;
    m_default_subresource = false;
    return *this;
}

void ImageBarrier::Execute() {
    const VkImageLayout old_layout = m_image.Layout();

    if (m_src_stage == 0)
        m_src_stage = InferPipelineStageFlags(old_layout);

    if (m_dst_stage == 0)
        m_dst_stage = InferPipelineStageFlags(m_new_layout);

    if (m_src_access == 0)
        m_src_access = InferAccessFlags(old_layout, m_generated_src_access);

    if (m_dst_access == 0)
        m_dst_access = InferAccessFlags(m_new_layout, m_generated_dst_access);

    if (m_default_subresource) {
        m_subresource.aspectMask = GetFormatAspect(m_image.Format());
        m_subresource.baseArrayLayer = 0;
        m_subresource.layerCount = m_image.ArrayLayers();
        m_subresource.baseMipLevel = 0;
        m_subresource.levelCount = m_image.MipLevels();
        m_image.m_layout = m_new_layout;
    } else {
        m_image.m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkImageMemoryBarrier2 barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = m_src_stage,
        .srcAccessMask = m_src_access,
        .dstStageMask = m_dst_stage,
        .dstAccessMask = m_dst_access,
        .oldLayout = old_layout,
        .newLayout = m_new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_image.Image(),
        .subresourceRange = m_subresource,
    };

    VkDependencyInfo dependency {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2(m_cmd, &dependency);
}

// ======================== //
// ---- Command Buffer ---- //
// ======================== //

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

void CommandBuffer::BeginRendering(const VulkanImage &color_image, glm::vec4 clear_color) const {
    BeginRendering({
        ImageAttachment{
            .type = AttachmentType::Color,
            .image = color_image,
            .should_clear = true,
            .clear_color = clear_color,
        },
    });
}

void CommandBuffer::BeginRendering(const VulkanImage &color_image, const VulkanImage &depth_image, glm::vec4 clear_color, float clear_depth) const {
    BeginRendering({
        ImageAttachment{
            .type = AttachmentType::Color,
            .image = color_image,
            .should_clear = true,
            .clear_color = clear_color,
        },
        ImageAttachment{
            .type = AttachmentType::Depth,
            .image = depth_image,
            .should_clear = true,
            .clear_depth = clear_depth
        },
    });
}

void CommandBuffer::BeginRendering(const std::vector<ImageAttachment> &render_targets) const {
    if (render_targets.empty())
        throw std::runtime_error("Cannot begin rendering without any render targets!");
    
    // Assume other render targets have same size. Not doing that leads to undefined behavior!
    const VkExtent3D extent = render_targets[0].image.Extent();
    VkRect2D render_area {
        .offset = { .x = 0, .y = 0 },
        .extent = {
            .width = extent.width,
            .height = extent.height,
        },
    };
    BeginRendering(render_targets, render_area);
}

void CommandBuffer::BeginRendering(const std::vector<ImageAttachment> &render_targets, VkRect2D render_area) const {
    if (render_targets.empty())
        throw std::runtime_error("Cannot begin rendering without any render targets!");
    
    std::vector<VkRenderingAttachmentInfo> color_attachments {};
    std::vector<VkRenderingAttachmentInfo> depth_attachments {};
    std::vector<VkRenderingAttachmentInfo> stencil_attachments {};

    for (const auto &attachment : render_targets) {
        VkRenderingAttachmentInfo attachment_info {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = attachment.image.View(),
            .imageLayout = attachment.image.Layout(),
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        if (attachment.should_clear) {
            attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            if (attachment.type == AttachmentType::Depth || attachment.type == AttachmentType::Stencil) {
                attachment_info.clearValue.depthStencil = {
                    .depth = attachment.clear_depth,
                    .stencil = attachment.clear_stencil,
                };
            } else if (attachment.type == AttachmentType::Color) {
                const glm::vec4 clear_color = attachment.clear_color;
                attachment_info.clearValue.color = {
                    .float32 = { clear_color.x, clear_color.y, clear_color.z, clear_color.w }
                };
            }
        }

        if (attachment.msaa && attachment.image.SampleCount() > 1u) {
            if (attachment.resolve_image == nullptr)
                throw std::runtime_error("Cannot resolve multisampled image with MSAA enabled to null image.");

            attachment_info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
            attachment_info.resolveImageLayout = attachment.resolve_image->Layout();
            attachment_info.resolveImageView = attachment.resolve_image->View();
        }

        if (attachment.type == AttachmentType::Color)
            color_attachments.push_back(attachment_info);
        else if (attachment.type == AttachmentType::Depth)
            depth_attachments.push_back(attachment_info);
        else if (attachment.type == AttachmentType::Stencil)
            stencil_attachments.push_back(attachment_info);
    }

    VkRenderingInfo rendering_info {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderArea = render_area,
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
        .pColorAttachments = color_attachments.data(),
        .pDepthAttachment = depth_attachments.empty() ? nullptr : &depth_attachments[0],
        .pStencilAttachment = stencil_attachments.empty() ? nullptr : &stencil_attachments[0],
    };

    vkCmdBeginRendering(m_command_buffer, &rendering_info);
}

void CommandBuffer::EndRendering() const {
    vkCmdEndRendering(m_command_buffer);
}

void CommandBuffer::BindPipeline(const VulkanPipeline &pipeline) const {
    vkCmdBindPipeline(m_command_buffer, pipeline.BindPoint(), pipeline.Pipeline());
}

void CommandBuffer::SetViewportAndScissor(glm::ivec2 offset, glm::uvec2 extent) const {
    VkViewport viewport {
        .x = static_cast<float>(offset.x),
        .y = static_cast<float>(offset.y),
        .width = static_cast<float>(extent.x),
        .height = static_cast<float>(extent.y),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor {
        .offset = { .x = 0, .y = 0 },
        .extent = { .width = extent.x, .height = extent.y },
    };

    vkCmdSetViewport(m_command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(m_command_buffer, 0, 1, &scissor);
}

void CommandBuffer::CopyImage(const VulkanImage &src, const VulkanImage &dst) {
    Assert(src.Extent().width == dst.Extent().width
        && src.Extent().height == dst.Extent().height
        && src.Extent().depth == dst.Extent().depth, "Cannot copy images of different extents!");
    VkImageCopy region {
        .srcSubresource = {
            .aspectMask = GetFormatAspect(src.Format()),
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = src.ArrayLayers(),
        },
        .srcOffset = VkOffset3D{},
        .dstSubresource = {
            .aspectMask = GetFormatAspect(src.Format()),
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = dst.ArrayLayers(),
        },
        .dstOffset = VkOffset3D{},
        .extent = src.Extent(),
    };
    vkCmdCopyImage(m_command_buffer, src.Image(), src.Layout(), dst.Image(), dst.Layout(), 1, &region);
}

void CommandBuffer::BindDescriptorSet(uint32_t set, const VulkanPipeline &pipeline, VkDescriptorSet descriptor_set) {
    vkCmdBindDescriptorSets(m_command_buffer, pipeline.BindPoint(), pipeline.Layout(), set, 1, &descriptor_set, 0, nullptr);
}

void CommandBuffer::BeginLabel(const std::string &label, glm::vec4 color) {
    if (!m_command_pool.m_device.EnabledValidations())
        return;

    VkDebugUtilsLabelEXT label_info {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = label.c_str(),
        .color = {
            color.r,
            color.g,
            color.b,
            color.a,
        },
    };
    vkCmdBeginDebugUtilsLabelEXT(m_command_buffer, &label_info);
}

void CommandBuffer::EndLabel() {
    if (!m_command_pool.m_device.EnabledValidations())
        return;

    vkCmdEndDebugUtilsLabelEXT(m_command_buffer);
}

void CommandBuffer::InsertLabel(const std::string &label, glm::vec4 color) {
    if (!m_command_pool.m_device.EnabledValidations())
        return;

    VkDebugUtilsLabelEXT label_info {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = label.c_str(),
        .color = {
            color.r,
            color.g,
            color.b,
            color.a,
        },
    };
    vkCmdInsertDebugUtilsLabelEXT(m_command_buffer, &label_info);
}

void CommandBuffer::BindVertexBuffer(VkBuffer buffer, VkDeviceSize offset) {
    vkCmdBindVertexBuffers(m_command_buffer, 0, 1, &buffer, &offset);
}

void CommandBuffer::BindIndexBuffer(VkBuffer buffer, VkDeviceSize offset) {
    vkCmdBindIndexBuffer(m_command_buffer, buffer, offset, VK_INDEX_TYPE_UINT32);
}

void CommandBuffer::Draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const {
    vkCmdDraw(m_command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void CommandBuffer::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) const {
    vkCmdDrawIndexed(m_command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void CommandBuffer::SetDebugName(std::string_view name) const {
    m_command_pool.m_device.SetDebugName(m_command_buffer, name);
}

void GenerateMipMaps(VulkanImage &image, VkFilter filter) {
    
}


ImageBarrier CommandBuffer::ImageMemoryBarrier(VulkanImage &image) const {
    return ImageBarrier(m_command_buffer, image);
}

void CommandBuffer::TransitionLayout(VulkanImage &image, VkImageLayout image_layout) const {
    ImageMemoryBarrier(image)
        .TransitionLayout(image_layout)
        .Execute();
}

void CommandBuffer::TransitionLayout(VulkanImage &image, VkImageLayout image_layout, const VkImageSubresourceRange &range) const {
    ImageMemoryBarrier(image)
        .TransitionLayout(image_layout)
        .SubresourceRange(range)
        .Execute();
}

void CommandBuffer::GenerateMipMaps(VulkanImage &image, VkFilter filter, uint32_t layer) const {
    Assert(image.MipLevels() > 1, "Should not generate mipmaps with just one mip level.");
    Assert(image.SampleCount() == 1u, "Cannot generate mipmaps for multi-sampled image!");
    Assert(image.Usage() & VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "Cannot generate mipmaps for image without usage TRANSFER_SRC_BIT!");
    
    const VkImageLayout old_layout = image.Layout();
    const VkExtent3D &extent = image.Extent();
    const VkImageAspectFlags aspect = GetFormatAspect(image.Format());

    for (uint32_t level = 1; level < image.MipLevels(); ++level) {
        // Transition previous level to TRANSFER_SRC and current level to TRANSFER_DST
        TransitionLayout(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VkImageSubresourceRange {
            .aspectMask = aspect,
            .baseMipLevel = level - 1,
            .levelCount = 1,
            .baseArrayLayer = layer,
            .layerCount = 1,
        });

        TransitionLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VkImageSubresourceRange {
            .aspectMask = aspect,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = layer,
            .layerCount = 1,
        });

        // Layer L will have dimensions floor(W / 2^L) by floor(H / 2^L) where W and H were
        // the base layer's Width and Height, respectively
        VkImageBlit2 blit_region {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .srcSubresource = {
                .aspectMask = aspect,
                .mipLevel = level - 1,
                .baseArrayLayer = layer,
                .layerCount = 1,
            },
            .srcOffsets = {
                VkOffset3D { .x = 0, .y = 0, .z = 0 },
                VkOffset3D {
                    .x = static_cast<int32_t>(extent.width) >> (level - 1),
                    .y = static_cast<int32_t>(extent.height) >> (level - 1),
                    .z = 1
                }
            },
            .dstSubresource = {
                .aspectMask = aspect,
                .mipLevel = level,
                .baseArrayLayer = layer,
                .layerCount = 1,
            },
            .dstOffsets = {
                VkOffset3D { .x = 0, .y = 0, .z = 0 },
                VkOffset3D {
                    .x = static_cast<int32_t>(extent.width) >> level,
                    .y = static_cast<int32_t>(extent.height) >> level,
                    .z = 1
                },
            },
        };
        
        VkBlitImageInfo2 blit_image_info {
            .sType   = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = image.Image(),
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = image.Image(),
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &blit_region,
            .filter = filter,
        };
        vkCmdBlitImage2(m_command_buffer, &blit_image_info);

        TransitionLayout(image, old_layout, VkImageSubresourceRange {
            .aspectMask = aspect,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = layer,
            .layerCount = 1,
        });
    }
}

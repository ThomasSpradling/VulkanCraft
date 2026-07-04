#include "VulkanImage.h"
#include "Common.h"
#include "VulkanBuffer.h"
#include <algorithm>
#include <format>
#include <memory>

// ============================== //
// ---- Vulkan Image Builder ---- //
// ============================== //

VulkanImageBuilder &VulkanImageBuilder::Image2D(uint32_t width, uint32_t height) {
    m_type = VK_IMAGE_TYPE_2D;
    m_view_type = VK_IMAGE_VIEW_TYPE_2D;
    m_extent = VkExtent3D{
        .width = width,
        .height = height,
        .depth = 0,
    };
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::Image3D(uint32_t width, uint32_t height, uint32_t depth) {
    m_type = VK_IMAGE_TYPE_3D;
    m_view_type = VK_IMAGE_VIEW_TYPE_3D;
    m_extent = VkExtent3D{
        .width = width,
        .height = height,
        .depth = depth,
    };
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::Image2DArray(uint32_t width, uint32_t height, uint32_t layers) {
    m_type = VK_IMAGE_TYPE_2D;
    m_view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    m_extent = VkExtent3D{
        .width = width,
        .height = height,
        .depth = 0,
    };
    m_array_layers = layers;
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::CubeMap(uint32_t face_width, uint32_t face_height) {
    m_type = VK_IMAGE_TYPE_2D;
    m_view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    m_extent = VkExtent3D{
        .width = face_width,
        .height = face_height,
        .depth = 0,
    };
    m_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::CubeMapArray(uint32_t face_width, uint32_t face_height, uint32_t layers) {
    m_type = VK_IMAGE_TYPE_2D;
    m_view_type = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    m_extent = VkExtent3D{
        .width = face_width,
        .height = face_height,
        .depth = 0,
    };
    m_array_layers = layers;
    m_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::Format(VkFormat format) {
    m_format = format;
    return *this;
}
VulkanImageBuilder &VulkanImageBuilder::MipMapLevels(uint32_t levels) {    
    m_mip_levels = levels;
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::SampleCount(uint32_t sample_count) {
    switch (sample_count) {
        case 1:
            m_sample_count = VK_SAMPLE_COUNT_1_BIT;
            break;
        case 2:
            m_sample_count = VK_SAMPLE_COUNT_2_BIT;
            break;
        case 4:
            m_sample_count = VK_SAMPLE_COUNT_4_BIT;
            break;
        case 8:
            m_sample_count = VK_SAMPLE_COUNT_8_BIT;
            break;
        case 16:
            m_sample_count = VK_SAMPLE_COUNT_16_BIT;
            break;
        case 32:
            m_sample_count = VK_SAMPLE_COUNT_32_BIT;
            break;
        case 64:
            m_sample_count = VK_SAMPLE_COUNT_64_BIT;
            break;
        default:
            Assert(false, "Invalid sample count for VulkanImage! It must be a power of two between 1 and 64.");
    }

    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::AddUsage(VkBufferUsageFlags usage) {
    m_usage = usage;
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::DedicateMemory() {
    m_memory_flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::AddMemoryFlags(VmaAllocationCreateFlags flag) {
    m_memory_flags |= flag;
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::ImageLayout(VkImageLayout image_layout) {
    m_layout = image_layout;
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::SharedQueueFamilies(std::span<uint32_t> queues) {
    m_queue_families = queues;
    return *this;
}

std::unique_ptr<VulkanImage> VulkanImageBuilder::Build(const VulkanDevice &device) {
    VkSharingMode sharing_mode = m_queue_families.size() >= 2 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    
    if (m_mip_levels == MaxMipmaps) {
        m_mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max({m_extent.width, m_extent.height, m_extent.depth})))) + 1;
    }
    
    VkImageCreateInfo image_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = m_flags,
        .imageType = m_type,
        .format = m_format,
        .extent = m_extent,
        .mipLevels = m_mip_levels,
        .arrayLayers = m_array_layers,
        .samples = m_sample_count,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = m_usage,
        .sharingMode = sharing_mode,
        .initialLayout = m_layout,
    };

    if (m_queue_families.size() >= 2) {
        image_create_info.pQueueFamilyIndices = m_queue_families.data();
        image_create_info.queueFamilyIndexCount = static_cast<uint32_t>(m_queue_families.size());
    }

    VmaAllocationCreateInfo allocation_create_info {
        .flags = m_memory_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkImage image;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
    VK_CHECK(vmaCreateImage(device.Allocator(), &image_create_info, &allocation_create_info, &image, &allocation, &allocation_info));

    auto vk_image = std::make_unique<VulkanImage>(device);

    vk_image->m_image = image;

    vk_image->m_allocation = allocation;
    vk_image->m_allocation_info = allocation_info;

    vk_image->m_type = m_type;
    vk_image->m_view_type = m_view_type;
    vk_image->m_full_subresource = {
        .aspectMask = GetFormatAspect(m_format),
        .baseMipLevel = 0,
        .levelCount = m_mip_levels,
        .baseArrayLayer = 0,
        .layerCount = m_array_layers,
    };

    vk_image->m_extent = m_extent;
    vk_image->m_array_layers = m_array_layers;
    vk_image->m_mip_levels = m_mip_levels;
    vk_image->m_sample_count = m_sample_count;

    vk_image->m_format = m_format;
    vk_image->m_layout = m_layout;

    vk_image->m_usage = m_usage;
    vk_image->m_flags = m_flags;
    vk_image->m_memory_flags = m_memory_flags;
    vk_image->m_queue_families = m_queue_families;
    vk_image->m_image_view = vk_image->CreateImageView();

    return vk_image;
}

// ============================== //
// ---- Vulkan Image Builder ---- //
// ============================== //

ImageBarrier::ImageBarrier(VulkanImage &image)
    : m_image(image)
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
    m_dst_access |= m_dst_access;
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

ImageBarrier &ImageBarrier::SubresourceRange(VkImageSubresourceRange subresource) {
    m_subresource = subresource;
    return *this;
}

void ImageBarrier::Execute(VkCommandBuffer cmd) {
    if (m_src_stage == 0)
        m_src_stage = GetPipelineStageFlags(m_image.Layout());

    if (m_dst_stage == 0)
        m_dst_stage = GetPipelineStageFlags(m_new_layout);

    if (m_src_access == 0)
        m_src_access = GetAccessFlags(m_image.Layout(), m_generated_src_access);

    if (m_dst_access == 0)
        m_dst_access = GetAccessFlags(m_new_layout, m_generated_dst_access);

    VkImageMemoryBarrier2 barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = m_src_stage,
        .srcAccessMask = m_src_access,
        .dstStageMask = m_dst_stage,
        .dstAccessMask = m_dst_access,
        .image = m_image.Image(),
        .subresourceRange = m_subresource,
    };
    
    if (m_new_layout != VK_IMAGE_LAYOUT_UNDEFINED && m_new_layout != m_image.m_layout) {
        barrier.oldLayout = m_image.Layout();
        barrier.newLayout = m_new_layout;
        m_image.m_layout = m_new_layout;
    }

    VkDependencyInfo dependency {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dependency);
}

// ====================== //
// ---- Vulkan Image ---- //
// ====================== //

VulkanImage::VulkanImage(const VulkanDevice &device)
    : m_device(device)
{}

VulkanImage::~VulkanImage() {
    if (m_image != VK_NULL_HANDLE && m_allocation != nullptr) {
        vmaDestroyImage(m_device.Allocator(), m_image, m_allocation);
    }

    if (m_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device.Device(), m_image_view, nullptr);
    }
}

std::unique_ptr<VulkanImage> VulkanImage::ExternalImage2D(const VulkanDevice &device, VkImage image, glm::ivec2 size, VkFormat format, uint32_t layers, VkImageUsageFlags usage) {
    auto result = std::make_unique<VulkanImage>(device);
    result->m_image = image;
    result->m_extent = {
        .width = static_cast<uint32_t>(size.x),
        .height = static_cast<uint32_t>(size.y),
    };
    result->m_format = format;
    result->m_array_layers = layers;
    result->m_usage = usage;
    result->m_image_view = result->CreateImageView();
    return result;
}

VkImageView VulkanImage::CreateImageView() {
     const VkImageSubresourceRange default_subresource = VkImageSubresourceRange{
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
    return CreateImageView(default_subresource);
}

VkImageView VulkanImage::CreateImageView(VkImageSubresourceRange subresource_range) {
    if (subresource_range.aspectMask == VK_IMAGE_ASPECT_NONE)
        subresource_range.aspectMask = GetFormatAspect(m_format);

    if (subresource_range.layerCount == VK_REMAINING_ARRAY_LAYERS)
        subresource_range.layerCount = m_array_layers;

    if (subresource_range.levelCount == VK_REMAINING_MIP_LEVELS)
        subresource_range.levelCount = m_mip_levels;
    
    VkImageViewCreateInfo image_view_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_image,
        .viewType = m_view_type,
        .format = m_format,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A,
        },
        .subresourceRange = subresource_range,
    };

    VkImageView image_view;
    VK_CHECK(vkCreateImageView(m_device.Device(), &image_view_create_info, nullptr, &image_view));
    return image_view;
}

void VulkanImage::Upload(const void *data, VkDeviceSize bytes, VkImageLayout image_layout) {
    UploadLayers(data, bytes, 0, m_array_layers, image_layout);
}

void VulkanImage::UploadLayer(const void *data, VkDeviceSize bytes, uint32_t layer, VkImageLayout image_layout) {
    UploadLayers(data, bytes, layer, 1, image_layout);
}

void VulkanImage::TransitionLayout(VkCommandBuffer cmd, VkImageLayout image_layout) {
    ImageBarrier(*this)
        .TransitionLayout(image_layout)
        .Execute(cmd);
}

void VulkanImage::TransitionLayout(VkCommandBuffer cmd, VkImageLayout image_layout, VkImageSubresourceRange range) {
    ImageBarrier(*this)
        .TransitionLayout(image_layout)
        .SubresourceRange(range)
        .Execute(cmd);
}

void VulkanImage::GenerateMipMaps(VkCommandBuffer cmd, VkFilter filter) {
    Assert(m_sample_count == VK_SAMPLE_COUNT_1_BIT, "Cannot generate mipmaps for multi-sampled image!");
    Assert(m_usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "Cannot generate mipmaps for image without usage TRANSFER_SRC_BIT!");
    for (uint32_t layer = 0; layer < m_array_layers; ++layer) {
        for (uint32_t level = 1; level < m_mip_levels; ++level) {
            // Transition previous level to TRANSFER_SRC and current level to TRANSFER_DST
            TransitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VkImageSubresourceRange{
                .aspectMask = GetFormatAspect(m_format),
                .baseMipLevel = level - 1,
                .levelCount = 1,
                .baseArrayLayer = layer,
                .layerCount = 1,
            });

            TransitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VkImageSubresourceRange{
                .aspectMask = GetFormatAspect(m_format),
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
                    .aspectMask = GetFormatAspect(m_format),
                    .mipLevel = level - 1,
                    .baseArrayLayer = layer,
                    .layerCount = 1,
                },
                .srcOffsets = {
                    VkOffset3D { .x = 0, .y = 0, .z = 0 },
                    VkOffset3D {
                        .x = static_cast<int32_t>(m_extent.width) >> (level - 1),
                        .y = static_cast<int32_t>(m_extent.height) >> (level - 1),
                        .z = 1
                    }
                },
                .dstSubresource = {
                    .aspectMask = GetFormatAspect(m_format),
                    .mipLevel = level,
                    .baseArrayLayer = layer,
                    .layerCount = 1,
                },
                .dstOffsets = {
                    VkOffset3D { .x = 0, .y = 0, .z = 0 },
                    VkOffset3D {
                        .x = static_cast<int32_t>(m_extent.width) >> level,
                        .y = static_cast<int32_t>(m_extent.height) >> level,
                        .z = 1
                    },
                },
            };
            
            VkBlitImageInfo2 blit_image_info {
                .sType   = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                .srcImage = m_image,
                .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .dstImage = m_image,
                .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .regionCount = 1,
                .pRegions = &blit_region,
                .filter = filter,
            };
            vkCmdBlitImage2(cmd, &blit_image_info);
        }
    }
    TransitionLayout(cmd, m_layout);
}

void VulkanImage::UploadLayers(const void *data, VkDeviceSize bytes, uint32_t layer, uint32_t layer_count, VkImageLayout image_layout) {
    VkDeviceSize source_bytes = GetBytesPerPixel(m_format) * m_extent.width * m_extent.height * m_extent.depth * layer_count;
    Assert(bytes == source_bytes, std::format("Non-matching sizes! Source data was {} bytes, but target data was"
        "[{} x {} x {}] x {} bytes x {} layers = {} bytes!", 
        bytes, m_extent.width, m_extent.height, m_extent.depth,
        GetBytesPerPixel(m_format), layer_count, source_bytes));

    if (image_layout == VK_IMAGE_LAYOUT_UNDEFINED)
        image_layout = m_layout;

    auto staging_buffer = VulkanBuffer::BufferBuilder()
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Build(m_device);

    std::memcpy(staging_buffer->Mapped(), data, bytes);

    m_device.ImmediateSubmit(QueueType::Graphics, [&](VkCommandBuffer cmd) {
        VkBufferImageCopy copy_region {
            .imageSubresource = {
                .aspectMask = GetFormatAspect(m_format),
                .mipLevel = 0,
                .baseArrayLayer = layer,
                .layerCount = layer_count,
            },
            .imageOffset = VkOffset3D { .x = 0, .y = 0, .z = 0 },
            .imageExtent = m_extent,
        };
        vkCmdCopyBufferToImage(cmd, staging_buffer->Buffer(), m_image, image_layout, 1, &copy_region);
    });
    m_layout = image_layout;
}

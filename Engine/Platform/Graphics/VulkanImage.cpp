#include "VulkanImage.h"
#include "Common.h"
#include "Core/errors.h"
#include "VulkanBuffer.h"
#include <algorithm>
#include <format>
#include <future>
#include <memory>
#include "CommandBuffer.h"
#include "VulkanDevice.h"

// ============================== //
// ---- Vulkan Image Builder ---- //
// ============================== //

VulkanImageBuilder &VulkanImageBuilder::Image2D(uint32_t width, uint32_t height) {
    m_type = VK_IMAGE_TYPE_2D;
    m_view_type = VK_IMAGE_VIEW_TYPE_2D;
    m_extent = VkExtent3D{
        .width = width,
        .height = height,
        .depth = 1,
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
        .depth = 1,
    };
    m_array_layers = layers;
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::CubeMap(uint32_t side_length) {
    m_type = VK_IMAGE_TYPE_2D;
    m_view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    m_extent = VkExtent3D{
        .width = side_length,
        .height = side_length,
        .depth = 1,
    };
    m_array_layers = 6;
    m_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::CubeMapArray(uint32_t side_length, uint32_t layers) {
    m_type = VK_IMAGE_TYPE_2D;
    m_view_type = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    m_extent = VkExtent3D{
        .width = side_length,
        .height = side_length,
        .depth = 1,
    };
    m_array_layers = layers * 6;
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
    m_sample_count = sample_count;
    return *this;
}

VulkanImageBuilder &VulkanImageBuilder::AddUsage(VkImageUsageFlags usage) {
    m_usage |= usage;
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

std::unique_ptr<VulkanImage> VulkanImageBuilder::Build() {
    VkSharingMode sharing_mode = m_queue_families.size() >= 2 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    
    ENGINE_ASSERT(m_format, "Image must have a format!");

    if (m_mip_levels == AutoComputeMipLevels) {
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
        .samples = GetSampleCount(m_sample_count),
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
    VK_CHECK(vmaCreateImage(m_device.Allocator(), &image_create_info, &allocation_create_info, &image, &allocation, &allocation_info));

    auto vk_image = std::make_unique<VulkanImage>(m_device);

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

// ====================== //
// ---- Vulkan Image ---- //
// ====================== //

VulkanImage::VulkanImage(const VulkanDevice &device)
    : m_device(device)
{}

VulkanImage::~VulkanImage() {
    if (m_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device.Device(), m_image_view, nullptr);
    }

    if (m_image != VK_NULL_HANDLE && m_allocation != nullptr) {
        vmaDestroyImage(m_device.Allocator(), m_image, m_allocation);
    }
}

std::unique_ptr<VulkanImage> VulkanImage::ExternalImage2D(const VulkanDevice &device, VkImage image, glm::ivec2 size, VkFormat format, uint32_t layers, VkImageUsageFlags usage) {
    auto result = std::make_unique<VulkanImage>(device);
    result->m_image = image;
    result->m_extent = {
        .width = static_cast<uint32_t>(size.x),
        .height = static_cast<uint32_t>(size.y),
        .depth = 1,
    };
    result->m_format = format;
    result->m_array_layers = layers;
    result->m_usage = usage;
    result->m_image_view = result->CreateImageView();
    return result;
}

void VulkanImage::SetDebugName(std::string_view name) const {
    m_device.SetDebugName(m_image, name);

    if (m_image_view) {
        std::string view_debug_name = std::string(name) + " View";
        m_device.SetDebugName(m_image_view, view_debug_name);
    }
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

void VulkanImage::Upload(const void *data, VkDeviceSize bytes) {
    UploadLayers(data, bytes, 0, m_array_layers);
}

void VulkanImage::Upload(const CommandBuffer &cmd, TextureRange range, const void *data, uint32_t buffer_row_length) {
    ENGINE_ASSERT(data != nullptr, "Cannot upload null texture data.");
    ENGINE_ASSERT(glm::all(glm::greaterThan(range.dimensions, glm::uvec3(0))), "Texture upload dimensions must be non-zero.");
    ENGINE_ASSERT(glm::all(glm::greaterThanEqual(range.offset, glm::ivec3(0))), "Texture upload offset cannot be negative.");
    ENGINE_ASSERT(range.num_layers > 0, "Texture upload must contain at least one layer.");
    ENGINE_ASSERT(range.num_mip_levels > 0, "Texture upload must contain at least one mip level.");

    ENGINE_ASSERT(range.layer < m_array_layers && range.num_layers <= m_array_layers - range.layer,
        std::format("Texture layer range [{}..{}) exceeds the image's {} layers.", range.layer, range.layer + range.num_layers, m_array_layers));

    ENGINE_ASSERT(range.mip_levels < m_mip_levels && range.num_mip_levels <= m_mip_levels - range.mip_levels,
        std::format("Texture mip range [{}..{}) exceeds the image's {} mip levels.", range.mip_levels,
            range.mip_levels + range.num_mip_levels, m_mip_levels));

    ENGINE_ASSERT(m_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || m_layout == VK_IMAGE_LAYOUT_GENERAL ||
        m_layout == VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
        "Cannot upload image unless its layout is TRANSFER_DST_OPTIMAL, GENERAL, or SHARED_PRESENT_KHR.");

    const auto mip_dimension = [](uint32_t value, uint32_t mip) { return std::max(1u, value >> mip); };
    const auto mip_offset = [](int32_t value, uint32_t mip) { return static_cast<int32_t>(static_cast<uint32_t>(value) >> mip); };
    const auto image_mip_extent = [&](uint32_t mip) {
        return VkExtent3D {
            .width = mip_dimension(m_extent.width, mip),
            .height = mip_dimension(m_extent.height, mip),
            .depth = mip_dimension(m_extent.depth, mip),
        };
    };

    const VkDeviceSize bytes_per_pixel = GetBytesPerPixel(m_format);
    VkDeviceSize staging_size = 0;

    std::vector<VkBufferImageCopy> copy_regions;
    copy_regions.reserve(range.num_mip_levels);

    for (uint32_t relative_mip = 0; relative_mip < range.num_mip_levels; ++relative_mip) {
        const uint32_t mip = range.mip_levels + relative_mip;

        const VkOffset3D offset {
            .x = mip_offset(range.offset.x, relative_mip),
            .y = mip_offset(range.offset.y, relative_mip),
            .z = mip_offset(range.offset.z, relative_mip),
        };

        const VkExtent3D dimensions {
            .width = mip_dimension(range.dimensions.x, relative_mip),
            .height = mip_dimension(range.dimensions.y, relative_mip),
            .depth = mip_dimension(range.dimensions.z, relative_mip),
        };

        const VkExtent3D mip_extent = image_mip_extent(mip);

        ENGINE_ASSERT(static_cast<uint64_t>(offset.x) + dimensions.width <= mip_extent.width &&
                static_cast<uint64_t>(offset.y) + dimensions.height <= mip_extent.height &&
                static_cast<uint64_t>(offset.z) + dimensions.depth <= mip_extent.depth,
            std::format("Texture region [{}, {}, {}] + [{} x {} x {}] exceeds mip {} extent [{} x {} x {}].",
                offset.x, offset.y, offset.z, dimensions.width, dimensions.height, dimensions.depth,
                mip, mip_extent.width, mip_extent.height, mip_extent.depth));

        const uint32_t row_length = buffer_row_length == 0 ? 0 : mip_dimension(buffer_row_length, relative_mip);
        ENGINE_ASSERT(row_length == 0 || row_length >= dimensions.width,
            std::format("Buffer row length {} is smaller than mip {} copy width {}.", row_length, mip, dimensions.width));

        const VkDeviceSize source_row_length = row_length == 0 ? dimensions.width : row_length;
        const VkDeviceSize mip_size = source_row_length * dimensions.height * dimensions.depth * range.num_layers * bytes_per_pixel;

        copy_regions.push_back(VkBufferImageCopy {
            .bufferOffset = staging_size,
            .bufferRowLength = row_length,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = GetFormatAspect(m_format),
                .mipLevel = mip,
                .baseArrayLayer = range.layer,
                .layerCount = range.num_layers,
            },
            .imageOffset = offset,
            .imageExtent = dimensions,
        });

        staging_size += mip_size;
    }

    auto staging_buffer = VulkanBuffer::BufferBuilder(m_device)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Size(staging_size)
        .Build();

    std::memcpy(staging_buffer->Mapped(), data, static_cast<size_t>(staging_size));
    staging_buffer->FlushMappedMemory(0, staging_size);

    vkCmdCopyBufferToImage(cmd.Handle(), staging_buffer->Buffer(), m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(copy_regions.size()), copy_regions.data());

    m_device.GetGarbageCollector().Enqueue(std::packaged_task<void()>([staging_buffer = std::move(staging_buffer)]() mutable {
        staging_buffer.reset();
    }));
}

void VulkanImage::Upload(TextureRange range, const void *data, uint32_t buffer_row_length) {
    m_device.ImmediateSubmit(QueueType::Graphics, [&](const CommandBuffer &cmd) {
        Upload(cmd, range, data, buffer_row_length);
    });
}

void VulkanImage::UploadLayer(const void *data, VkDeviceSize bytes, uint32_t layer) {
    UploadLayers(data, bytes, layer, 1);
}

void VulkanImage::TransitionLayout(VkImageLayout layout) {
    m_device.ImmediateSubmit(QueueType::Graphics, [&](const CommandBuffer &cmd) {
        cmd.TransitionLayout(*this, layout);
    });
}

void VulkanImage::UploadLayers(const void *data, VkDeviceSize bytes, uint32_t layer, uint32_t layer_count) {
    VkDeviceSize source_bytes = GetBytesPerPixel(m_format) * m_extent.width * m_extent.height * m_extent.depth * layer_count;
    ENGINE_ASSERT(bytes == source_bytes, std::format("Non-matching sizes! Source data was {} bytes, but target data was"
        "[{} x {} x {}] x {} bytes x {} layers = {} bytes!", 
        bytes, m_extent.width, m_extent.height, m_extent.depth,
        GetBytesPerPixel(m_format), layer_count, source_bytes));

    ENGINE_ASSERT(m_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        || m_layout == VK_IMAGE_LAYOUT_GENERAL
        || m_layout == VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR, "Cannot upload image unless layout is either TRANSFER_DST, GENERAL, or SHARED_PRESENT.");

    auto staging_buffer = VulkanBuffer::BufferBuilder(m_device)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Size(bytes)
        .Build();

    std::memcpy(staging_buffer->Mapped(), data, bytes);

    m_device.ImmediateSubmit(QueueType::Graphics, [&](const CommandBuffer &cmd) {
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
        vkCmdCopyBufferToImage(cmd.Handle(), staging_buffer->Buffer(), m_image, m_layout, 1, &copy_region);
    });
}

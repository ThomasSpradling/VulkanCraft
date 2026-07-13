#pragma once
#include "Common.h"
#include "VulkanDevice.h"
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
#include "VulkanBuffer.h"

constexpr uint32_t AutoComputeMipLevels = std::numeric_limits<uint32_t>::max();

class VulkanImage;
class VulkanImageBuilder {
    friend VulkanImage;
public:
    VulkanImageBuilder(const VulkanDevice &device) : m_device(device) {};

    VulkanImageBuilder &Image2D(uint32_t width, uint32_t height);
    VulkanImageBuilder &Image3D(uint32_t width, uint32_t height, uint32_t depth);
    VulkanImageBuilder &Image2DArray(uint32_t width, uint32_t height, uint32_t layers);
    VulkanImageBuilder &CubeMap(uint32_t side_length);
    VulkanImageBuilder &CubeMapArray(uint32_t side_length, uint32_t layers);

    VulkanImageBuilder &DepthImage();
    VulkanImageBuilder &DepthStencilImage();

    VulkanImageBuilder &ColorAttachment(bool hdr = false);

    VulkanImageBuilder &Format(VkFormat format);
    VulkanImageBuilder &MipMapLevels(uint32_t levels = AutoComputeMipLevels);
    inline VulkanImageBuilder &MipMaps() {
        return MipMapLevels(AutoComputeMipLevels);
    }

    VulkanImageBuilder &LayersCount(uint32_t layers);
    VulkanImageBuilder &SampleCount(uint32_t sample_count);

    VulkanImageBuilder &AddUsage(VkImageUsageFlags usage);

    // For larger data to ensure that this memory has its own dedicated
    // memory block.
    VulkanImageBuilder &DedicateMemory();

    VulkanImageBuilder &AddMemoryFlags(VmaAllocationCreateFlags flag);
    VulkanImageBuilder &ImageLayout(VkImageLayout image_layout);

    VulkanImageBuilder &SharedQueueFamilies(std::span<uint32_t> queues);

    std::unique_ptr<VulkanImage> Build();
private:
    const VulkanDevice &m_device;

    VkImageType m_type = VkImageType::VK_IMAGE_TYPE_2D;
    VkImageViewType m_view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;

    VkExtent3D m_extent {};
    uint32_t m_array_layers = 1;
    uint32_t m_mip_levels = 1;

    uint32_t m_sample_count = 1u;

    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkImageLayout m_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageUsageFlags m_usage = 0;
    VkImageCreateFlags m_flags = 0;
    VmaAllocationCreateFlags m_memory_flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    std::span<uint32_t> m_queue_families {};
};

class ImageBarrier;

class VulkanImage {
    friend VulkanImageBuilder;
    friend ImageBarrier;
public:
    VulkanImage(const VulkanDevice &device);
    ~VulkanImage();

    // Creates a container for an externally-managed image (e.g. via a swapchain)
    static std::unique_ptr<VulkanImage> ExternalImage2D(const VulkanDevice &device, VkImage image, glm::ivec2 size, VkFormat format, uint32_t layers, VkImageUsageFlags usage);
    static inline VulkanImageBuilder ImageBuilder(const VulkanDevice &device) { return VulkanImageBuilder(device); }

    void SetDebugName(std::string_view name) const;

    // Memory must be managed by caller. Any parameters left empty
    // will be filled with reasonable defaults.
    VkImageView CreateImageView();
    VkImageView CreateImageView(VkImageSubresourceRange subresource_range);

    VkImageView View() const { return m_image_view; }

    VkImage Image() const { return m_image; }
    VmaAllocation Allocation() const { return m_allocation; }

    VkExtent3D Extent() const { return m_extent; }
    uint32_t ArrayLayers() const { return m_array_layers; }
    uint32_t MipLevels() const { return m_mip_levels; }

    VkFormat Format() const { return m_format; }
    VkImageLayout Layout() const { return m_layout; }
    uint32_t SampleCount() const { return m_sample_count; }

    VkImageUsageFlags Usage() const { return m_usage; }

    void Upload(const void *data, VkDeviceSize bytes);
    void UploadLayer(const void *data, VkDeviceSize bytes, uint32_t layer);

    void TransitionLayout(VkImageLayout layout);
private:
    const VulkanDevice &m_device;
private:
    VkImage m_image = VK_NULL_HANDLE;
    VmaAllocation m_allocation = nullptr;
    VmaAllocationInfo m_allocation_info {};

    VkImageView m_image_view = VK_NULL_HANDLE;

    VkImageType m_type = VK_IMAGE_TYPE_2D;
    VkImageViewType m_view_type = VK_IMAGE_VIEW_TYPE_2D;

    VkImageSubresourceRange m_full_subresource {};

    VkExtent3D m_extent {};
    uint32_t m_array_layers = 1u;
    uint32_t m_mip_levels = 1u;

    uint32_t m_sample_count = 1u;

    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkImageLayout m_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageUsageFlags m_usage = 0;
    VkImageCreateFlags m_flags = 0;
    VmaAllocationCreateFlags m_memory_flags = 0;
    std::span<uint32_t> m_queue_families {};
private:
    void UploadLayers(const void *data, VkDeviceSize bytes, uint32_t layer, uint32_t layer_count);
};

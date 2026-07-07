#pragma once
#include "Common.h"
#include "VulkanDevice.h"
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <vector>

constexpr uint32_t MaxMipmaps = std::numeric_limits<uint32_t>::max();

class VulkanImage;
class VulkanImageBuilder {
    friend VulkanImage;
public:
    VulkanImageBuilder() = default;

    VulkanImageBuilder &Image2D(uint32_t width, uint32_t height);
    VulkanImageBuilder &Image3D(uint32_t width, uint32_t height, uint32_t depth);
    VulkanImageBuilder &Image2DArray(uint32_t width, uint32_t height, uint32_t layers);
    VulkanImageBuilder &CubeMap(uint32_t face_width, uint32_t face_height);
    VulkanImageBuilder &CubeMapArray(uint32_t face_width, uint32_t face_height, uint32_t layers);

    VulkanImageBuilder &DepthImage();
    VulkanImageBuilder &DepthStencilImage();

    VulkanImageBuilder &ColorAttachment(bool hdr = false);

    VulkanImageBuilder &Format(VkFormat format);
    VulkanImageBuilder &MipMapLevels(uint32_t levels = MaxMipmaps);
    inline VulkanImageBuilder &MipMaps() {
        return MipMapLevels(MaxMipmaps);
    }

    VulkanImageBuilder &LayersCount(uint32_t layers);
    VulkanImageBuilder &SampleCount(uint32_t sample_count);

    VulkanImageBuilder &AddUsage(VkBufferUsageFlags usage);

    // For larger data to ensure that this memory has its own dedicated
    // memory block.
    VulkanImageBuilder &DedicateMemory();

    VulkanImageBuilder &AddMemoryFlags(VmaAllocationCreateFlags flag);
    VulkanImageBuilder &ImageLayout(VkImageLayout image_layout);

    VulkanImageBuilder &SharedQueueFamilies(std::span<uint32_t> queues);

    std::unique_ptr<VulkanImage> Build(const VulkanDevice &device);
private:
    VkImageType m_type;
    VkImageViewType m_view_type;

    VkExtent3D m_extent {};
    uint32_t m_array_layers = 1;
    uint32_t m_mip_levels = 1;

    VkSampleCountFlagBits m_sample_count = VK_SAMPLE_COUNT_1_BIT;

    VkFormat m_format;
    VkImageLayout m_layout;

    VkImageUsageFlags m_usage = 0;
    VkImageCreateFlags m_flags = 0;
    VmaAllocationCreateFlags m_memory_flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    std::span<uint32_t> m_queue_families {};
};

class VulkanImage;
class ImageBarrier {
public:
    ImageBarrier(VulkanImage &image);

    // Selects a suitable access based on other parameters
    ImageBarrier &SourceAccess(MemoryAccessType access);
    ImageBarrier &DestAccess(MemoryAccessType access);
    
    ImageBarrier &SourceAccess(VkAccessFlags2 access);
    ImageBarrier &DestAccess(VkAccessFlags2 access);

    ImageBarrier &SourceStage(VkPipelineStageFlags2 stage);
    ImageBarrier &DestStage(VkPipelineStageFlags2 stage);

    ImageBarrier &TransitionLayout(VkImageLayout new_layout);
    ImageBarrier &SubresourceRange(VkImageSubresourceRange subresource);

    void Execute(VkCommandBuffer cmd);
private:
    VulkanImage &m_image;

    MemoryAccessType m_generated_src_access = MemoryAccessType::ReadWrite;
    MemoryAccessType m_generated_dst_access = MemoryAccessType::ReadWrite;

    VkAccessFlags2 m_src_access = 0;
    VkAccessFlags2 m_dst_access = 0;

    VkPipelineStageFlags2 m_src_stage = 0;
    VkPipelineStageFlags2 m_dst_stage = 0;

    VkImageLayout m_new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageSubresourceRange m_subresource {};
};

class VulkanImage {
    friend VulkanImageBuilder;
    friend ImageBarrier;
public:
    VulkanImage(const VulkanDevice &device);
    ~VulkanImage();

    // Creates a container for an externally-managed image (e.g. via a swapchain)
    static std::unique_ptr<VulkanImage> ExternalImage2D(const VulkanDevice &device, VkImage image, glm::ivec2 size, VkFormat format, uint32_t layers, VkImageUsageFlags usage);

    static inline VulkanImageBuilder ImageBuilder() { return VulkanImageBuilder(); }

    // Memory must be managed by caller. Any parameters left empty
    // will be filled with reasonable defaults.
    VkImageView CreateImageView();
    VkImageView CreateImageView(VkImageSubresourceRange subresource_range);

    VkImageView View() { return m_image_view; }

    VkImage Image() const { return m_image; }
    VmaAllocation Allocation() const { return m_allocation; }
    VkExtent3D Extent() const { return m_extent; }
    VkFormat Format() const { return m_format; }
    VkImageLayout Layout() const { return m_layout; }

    void Upload(const void *data, VkDeviceSize bytes, VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED);
    void UploadLayer(const void *data, VkDeviceSize bytes, uint32_t layer, VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED);

    void TransitionLayout(VkCommandBuffer cmd, VkImageLayout image_layout);
    void TransitionLayout(VkCommandBuffer cmd, VkImageLayout image_layout, VkImageSubresourceRange range);
    void GenerateMipMaps(VkCommandBuffer cmd, VkFilter filter = VK_FILTER_LINEAR);
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
    uint32_t m_array_layers = 1;
    uint32_t m_mip_levels = 1;

    VkSampleCountFlagBits m_sample_count = VK_SAMPLE_COUNT_1_BIT;

    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkImageLayout m_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageUsageFlags m_usage = 0;
    VkImageCreateFlags m_flags = 0;
    VmaAllocationCreateFlags m_memory_flags = 0;
    std::span<uint32_t> m_queue_families {};
private:
    void UploadLayers(const void *data, VkDeviceSize bytes, uint32_t layer, uint32_t layer_count, VkImageLayout image_layout);
};

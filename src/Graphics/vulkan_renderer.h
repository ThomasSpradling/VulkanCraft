#pragma once

#include "mesh.h"
#include "utils.h"

#include <GLFW/glfw3.h>
#include <cstdint>

#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>

struct RenderProperties {
    const uint32_t vulkan_version = VK_MAKE_API_VERSION(0, 1, 3, 0);

    bool enable_validation = true;

    VkExtent2D window_extent;
    bool enable_vsync = false;
};

struct Frame {
    // The graphic command buffer that executes commands on the current swapchain image
    VkCommandBuffer cmd;

    // Current swapchain image, a view into it, and its extent
    VkImage image;
    VkImageView image_view;
    VkExtent2D extent;
};

/**
 * @brief Handles the basic setup and some other utilities for Vulkan rendering.
 * This includes Vulkan initialization, swapchain creation, device selection, device
 * queue creation, and other utilities. Most static settings will be hardcoded here
 * (like version numbers).
 * 
 * @note This should NOT contain specific logic for graphics rendering. Render passes
 * can be created by a simple utility here, bu no specific logic should be implemented.
 */
class VulkanRenderer {
public:
    VulkanRenderer(GLFWwindow &window);
    ~VulkanRenderer();

    RenderProperties &Properties() { return m_props; }

    void Initialize();
    void ShutDown();

    // Waits on previous frame to finish and for the swapchain to give us the next valid image.
    std::optional<Frame> BeginFrame();

    // Signals completion of this frame so that a future frame may begin.
    void EndFrame();

    void ImmediateSubmit(std::function<void(VkCommandBuffer)> &&callback) const;
    
    uint32_t GetCurrentFrameIndex() const { return m_current_frame_index; }
    VkDevice GetDevice() const { return m_device; }
    VmaAllocator GetMemoryAllocator() const { return m_allocator; }
    VkDescriptorPool GetDescriptorPool() const { return m_descriptor_pool; }

    
    size_t SwapChainImageCount() const { return m_swapchain_images.size(); }
    
    VkExtent2D DrawExtent() const { return m_swapchain_extent; }
    
    VkFormat GetColorFormat() { return m_image_formats.color; }
    VkFormat GetDepthStencilFormat() { return m_image_formats.depth_stencil; }
    VkFormat GetDepthOnlyFormat() { return m_image_formats.depth; }
    VkFormat GetHDRFormat() { return m_image_formats.hdr; }

    VkShaderModule LoadShader(const std::string &file_path);

    // Upload CPU mesh data to GPU buffers. The buffers MUST then be managed by the caller.
    GPUMesh UploadGPUMesh(const std::vector<MeshVertex> &vertices, const std::vector<uint32_t> &indices) const;
    
    // Cleans up mesh data. Does NOT guarantee data-hazard safety.
    void DestroyGPUMesh(const GPUMesh &mesh) const;
public:
    // Loads the data into a buffer that has already been allocated. Offsets and sizes are represented in bytes.
    template <typename T>
    void LoadBufferData(VkBuffer buffer, std::vector<T> data, size_t buffer_offset = 0, size_t data_offset = 0, size_t size = 0) const {
        const size_t buffer_size = size == 0 ? sizeof(T) * data.size() : size;
        
        if (buffer == VK_NULL_HANDLE)
            throw std::runtime_error("Cannot load data into a null buffer.");

        // Create staging buffer
        VkBuffer staging_buffer;
        VmaAllocation staging_buffer_alloc;
        VmaAllocationInfo staging_allocation_info;
        {
            VkBufferCreateInfo buffer_create_info {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext = nullptr,
                .size = buffer_size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            VmaAllocationCreateInfo allocation_create_info {
                .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO,
            };

            VK_CHECK(vmaCreateBuffer(m_allocator, &buffer_create_info, &allocation_create_info, &staging_buffer, &staging_buffer_alloc, &staging_allocation_info));
        }

        // Copy raw data into staging buffer
        std::memcpy(staging_allocation_info.pMappedData, data.data(), buffer_size);

        ImmediateSubmit([&](VkCommandBuffer cmd) {
            VkBufferCopy copy_region {
                .srcOffset = data_offset,
                .dstOffset = buffer_offset,
                .size = buffer_size,
            };
            vkCmdCopyBuffer(cmd, staging_buffer, buffer, 1, &copy_region);
        });

        vmaDestroyBuffer(m_allocator, staging_buffer, staging_buffer_alloc);
    }
private:
    struct ImageFormats {
        VkFormat color;
        VkFormat depth_stencil;
        VkFormat depth;
        VkFormat hdr;
    };

    struct PerFrameData {
        VkSemaphore swapchain_acquire_semaphore;
        VkSemaphore draw_complete_semaphore;
        VkFence render_fence;
        
        VkCommandPool graphics_command_pool;
        VkCommandBuffer graphics_command_buffer;

        // Render target of all graphics operations: Will be transfered to swapchain directly
        VkImage draw_image;
        VkImageView draw_image_view;
        VmaAllocation draw_image_allocation;
    };

    const uint32_t DESCRIPTOR_POOL_MAX_SETS = 2048;
private:
    RenderProperties m_props;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;

    GLFWwindow &m_window;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    uint32_t m_graphics_queue_family = std::numeric_limits<uint32_t>::max();
    VkQueue m_graphics_queue = VK_NULL_HANDLE;

    // Immediate command buffer and pool
    VkCommandPool m_graphics_command_pool = VK_NULL_HANDLE;
    VkCommandBuffer m_graphics_command_buffer = VK_NULL_HANDLE;
    VkFence m_graphics_fence = VK_NULL_HANDLE;

    VmaAllocator m_allocator = VK_NULL_HANDLE;

    ImageFormats m_image_formats;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkExtent2D m_swapchain_extent;
    std::vector<VkImage> m_swapchain_images;

    bool m_swapchain_resize_requested = false;

    uint32_t m_swapchain_image_index = 0;

    std::vector<PerFrameData> m_frame_data;
    uint32_t m_current_frame_index = 0; // Current frame in flight

    VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;
private:
    void InitVulkanInstance();
    void DestroyVulkanInstance();
    
    void InitVulkanDevice();
    void DestroyVulkanDevice();

    void CreateSwapChain(const VkExtent2D &extent);
    void DestroySwapChain();
    void ResizeSwapChain();

    void InitFrameData();
    void DestroyFrameData();

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data);
};

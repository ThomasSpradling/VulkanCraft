#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>

#include <limits>
#include <optional>
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
private:
    struct ImageFormats {
        VkFormat color;
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

    VmaAllocator m_allocator = VK_NULL_HANDLE;

    ImageFormats m_image_formats;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkExtent2D m_swapchain_extent;
    std::vector<VkImage> m_swapchain_images;

    bool m_swapchain_resize_requested = false;

    uint32_t m_swapchain_image_index = 0;

    std::vector<PerFrameData> m_frame_data;
    uint32_t m_current_frame_index = 0; // Current frame in flight
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
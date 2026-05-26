#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>

#include <limits>
#include <vk_mem_alloc.h>

struct RenderProperties {
    const uint32_t vulkan_version = VK_MAKE_API_VERSION(0, 1, 3, 0);

    bool enable_validation = true;
    bool enable_vsync = false;
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
    VulkanRenderer();
    ~VulkanRenderer();

    void Initialize(GLFWwindow *window);
private:
    struct ImageFormats {
        VkFormat color;
        VkFormat hdr;
    };
private:
    RenderProperties m_props;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    uint32_t m_graphics_queue_family = std::numeric_limits<uint32_t>::max();
    VkQueue m_graphics_queue = VK_NULL_HANDLE;

    VmaAllocator m_allocator = VK_NULL_HANDLE;

    ImageFormats m_image_formats;
private:
    void InitVulkanInstance(GLFWwindow *window);
    void InitVulkanDevice();

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data);
};
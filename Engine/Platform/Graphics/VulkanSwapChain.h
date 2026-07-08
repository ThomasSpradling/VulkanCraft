#pragma once

#include "Common.h"
#include "Core/NonMovable.h"
#include "Core/NonCopyable.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"

#include <functional>
#include <glm/glm.hpp>
#include <optional>

struct SwapChainConfig {
    uint32_t width;
    uint32_t height;

    bool enable_vsync = false;
};

class VulkanSwapChain : public NonMovable, public NonCopyable {
public:
    VulkanSwapChain(const VulkanDevice &device, SwapChainConfig config);
    ~VulkanSwapChain();

    VulkanImage &CurrentImage();
    uint32_t CurrentImageIndex() const { return m_current_swapchain_image; }
    uint32_t GetImageCount();

    // Returns nullopt if it cannot be done successfully
    std::optional<uint32_t> AcquireNextImage(VkFence signal_fence, VkSemaphore signal_semaphore);

    // Returns whether this was done successfully
    bool Present(std::span<VkSemaphore> wait_semaphores = {});

    void Recreate(SwapChainConfig config);
private:
    struct ImageProperties {
        VkImageUsageFlags usage = 0;
        uint32_t array_layers = 1;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    };
private:
    const VulkanDevice &m_device;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

    ImageProperties m_image_properties {};
    SwapChainConfig m_config {};

    uint32_t m_current_swapchain_image = 0;
    std::vector<std::unique_ptr<VulkanImage>> m_swapchain_images {};
private:
    void CreateSwapChain();
    void DestroySwapChain();

    void CreateFrameData();
    void DestroyFrameData();
};

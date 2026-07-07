#include "VulkanSwapChain.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>

VulkanSwapChain::VulkanSwapChain(const VulkanDevice &device, SwapChainConfig config)
    : m_device(device)
    , m_config(std::move(config))
{
    CreateSwapChain();
    CreateFrameData();
    std::cout << "Created Vulkan Swap Chain.\n";
}

VulkanSwapChain::~VulkanSwapChain() {
    DestroyFrameData();
    DestroySwapChain();
    std::cout << "Destroyed Vulkan Swap Chain.\n";
}

std::optional<uint32_t> VulkanSwapChain::AcquireNextImage(VkFence signal_fence, VkSemaphore signal_semaphore) {
    VkAcquireNextImageInfoKHR next_image_info {
        .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
        .swapchain = m_swapchain,
        .timeout = UINT64_MAX,
        .semaphore = signal_semaphore,
        .fence = signal_fence,
        .deviceMask = 1,
    };
    uint32_t index;
    VkResult result = vkAcquireNextImage2KHR(m_device.Device(), &next_image_info, &index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
        return std::nullopt;
    if (result != VK_SUBOPTIMAL_KHR)
        VK_CHECK(result);
    m_current_swapchain_image = index;
    return index;
}

bool VulkanSwapChain::Present(std::span<VkSemaphore> wait_semaphores) {
    std::vector<uint32_t> image_index = { m_current_swapchain_image };
    VkPresentInfoKHR present_info {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size()),
        .pWaitSemaphores = wait_semaphores.data(),
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = image_index.data(),
        .pResults = nullptr,
    }; 
    VkResult result = vkQueuePresentKHR(m_device.Queue(QueueType::Present), &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        return false;
    VK_CHECK(result);
    return true;
}

VulkanImage &VulkanSwapChain::CurrentImage() {
    return *m_swapchain_images[m_current_swapchain_image];
}

uint32_t VulkanSwapChain::GetImageCount() {
    return static_cast<uint32_t>(m_swapchain_images.size());
}

void VulkanSwapChain::Recreate(SwapChainConfig config) {
    m_config = std::move(config);

    DestroyFrameData();
    CreateSwapChain();
    CreateFrameData();
}

void VulkanSwapChain::CreateSwapChain() {
    VkSwapchainKHR old_swapchain = m_swapchain;

    VkSurfaceCapabilitiesKHR surface_capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device.PhysicalDevice(), m_device.Surface(), &surface_capabilities));

    // Choose present format
    {
        uint32_t surface_formats_count = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.PhysicalDevice(), m_device.Surface(), &surface_formats_count, nullptr));
        Assert(surface_formats_count > 0, "Cannot find any surface formats.", __FILE__, __LINE__);

        std::vector<VkSurfaceFormatKHR> surface_formats(surface_formats_count);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.PhysicalDevice(), m_device.Surface(), &surface_formats_count, surface_formats.data()));
    
        bool found = false;
        for (const auto &format : surface_formats) {
            if (format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                m_image_properties.format = format.format;
                m_image_properties.color_space = format.colorSpace;
                found = true;
                break;
            }
        }
        
        if (!found) {
            std::cout << "Warning: Could not find sRGB color space for swapchain image!\n";
            m_image_properties.format = surface_formats[0].format;
            m_image_properties.color_space = surface_formats[0].colorSpace;
        }
    }

    // Choose present mode
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    {
        uint32_t present_modes_count = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_device.PhysicalDevice(), m_device.Surface(), &present_modes_count, nullptr));
        Assert(present_modes_count > 0, "Cannot find any present modes.", __FILE__, __LINE__);

    
        std::vector<VkPresentModeKHR> present_modes(present_modes_count);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_device.PhysicalDevice(), m_device.Surface(), &present_modes_count, present_modes.data()));
    
        if (m_config.enable_vsync) {
            // guaranteed by spec to be present
            present_mode = VK_PRESENT_MODE_FIFO_KHR;
        } else {
            std::vector<VkPresentModeKHR> preferred_present_modes {
                VK_PRESENT_MODE_IMMEDIATE_KHR,
                VK_PRESENT_MODE_MAILBOX_KHR,
                VK_PRESENT_MODE_FIFO_KHR,
            };

            for (auto mode : preferred_present_modes) {
                if (std::ranges::find(present_modes, mode) != present_modes.end()) {
                    present_mode = mode;
                    break;
                }
            }
        }

        std::string chosen_mode = "";
        switch (present_mode) {
            case VK_PRESENT_MODE_MAILBOX_KHR:
                chosen_mode = "MAILBOX";
                break;
            case VK_PRESENT_MODE_IMMEDIATE_KHR:
                chosen_mode = "IMMEDIATE";
                break;
            case VK_PRESENT_MODE_FIFO_KHR:
            default:
                chosen_mode = "FIFO";
                break;
        }
        std::cout << "Chose presentation mode: " << chosen_mode << "\n";
    }

    // Choose extent
    VkExtent2D current_extent = surface_capabilities.currentExtent;
    if (surface_capabilities.currentExtent.width == UINT32_MAX) {
        current_extent.width = std::clamp(m_config.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
        current_extent.height = std::clamp(m_config.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    }
    m_config.width = current_extent.width;
    m_config.height = current_extent.height;

    // Choose image count
    uint32_t image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount) {
        image_count = surface_capabilities.maxImageCount;
    }

    // Choose pre-transform
    VkSurfaceTransformFlagBitsKHR pre_transform = surface_capabilities.currentTransform;
    if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }

    // Choose swapchain blending mode
    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    {
        // Preferred order from top to bottom
        std::vector<VkCompositeAlphaFlagBitsKHR> composite_alpha_flags {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
    
        for (auto flag : composite_alpha_flags) {
            if (surface_capabilities.supportedCompositeAlpha & flag) {
                composite_alpha = flag;
                break;
            }
        }
    }

    VkSwapchainCreateInfoKHR swapchain_create_info {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .surface = m_device.Surface(),
        .minImageCount = image_count,
        .imageFormat = m_image_properties.format,
        .imageColorSpace = m_image_properties.color_space,
        .imageExtent = VkExtent2D{ .width = m_config.width, .height = m_config.height },
        .imageArrayLayers = 1,
        .imageUsage = 0,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = pre_transform,
        .compositeAlpha = composite_alpha,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain,
    };

    Assert(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "Surface does not support TRANSFER_SRC", __FILE__, __LINE__);
    Assert(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT, "Surface does not support TRANSFER_DST", __FILE__, __LINE__);
    Assert(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, "Surface does not support COLOR_ATTACHMENT", __FILE__, __LINE__);

    swapchain_create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapchain_create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchain_create_info.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    m_image_properties.array_layers = swapchain_create_info.imageArrayLayers;
    m_image_properties.usage = swapchain_create_info.imageUsage;

    VK_CHECK(vkCreateSwapchainKHR(m_device.Device(), &swapchain_create_info, nullptr, &m_swapchain));
    
    if (old_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device.Device(), old_swapchain, nullptr);
    }
}

void VulkanSwapChain::DestroySwapChain() {
    vkDestroySwapchainKHR(m_device.Device(), m_swapchain, nullptr);
}

void VulkanSwapChain::CreateFrameData() {
    // m_config.create_callback();

    //// Get Swapchain Images ////
    uint32_t image_count;
    VK_CHECK(vkGetSwapchainImagesKHR(m_device.Device(), m_swapchain, &image_count, nullptr));

    std::vector<VkImage> images(image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(m_device.Device(), m_swapchain, &image_count, images.data()));   

    m_swapchain_images.resize(image_count);
    for (uint32_t i = 0; i < image_count; ++i) {
        m_swapchain_images[i] = VulkanImage::ExternalImage2D(
            m_device,
            images[i],
            glm::ivec2(m_config.width, m_config.height),
            m_image_properties.format,
            m_image_properties.array_layers,
            m_image_properties.usage
        );
    }
}

void VulkanSwapChain::DestroyFrameData() {
    // m_config.destroy_callback();
    m_swapchain_images.clear();
}

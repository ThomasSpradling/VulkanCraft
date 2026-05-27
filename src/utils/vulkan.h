#pragma once

#include <volk.h>
#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>

#define VK_CHECK(expr) \
    if ((expr) != VK_SUCCESS) { \
        std::string str = "Call '" + std::string(#expr) + "' returned " + std::string(string_VkResult(expr)) + "."; \
        throw std::runtime_error(str.c_str()); \
    }

inline bool SupportsApiVersion(uint32_t actual_version, uint32_t requested_version) {
    uint32_t actual_variant = VK_API_VERSION_VARIANT(actual_version);
    uint32_t requested_variant = VK_API_VERSION_VARIANT(requested_version);
    if (actual_variant != requested_variant)
        return actual_variant > requested_variant;

    uint32_t actual_major = VK_API_VERSION_MAJOR(actual_version);
    uint32_t requested_major = VK_API_VERSION_MAJOR(requested_version);
    if (actual_major != requested_major)
        return actual_major > requested_major;

    uint32_t actual_minor = VK_API_VERSION_MINOR(actual_version);
    uint32_t requested_minor = VK_API_VERSION_MINOR(requested_version);
    if (actual_minor != requested_minor)
        return actual_minor > requested_minor;

    uint32_t actual_patch = VK_API_VERSION_MINOR(actual_version);
    uint32_t requested_patch = VK_API_VERSION_MINOR(requested_version);
    return actual_patch >= requested_patch;
}

/////////////////////////////////////////////
///  ---- TEMPLATE VULKAN STRUCTURES ---- ///
/////////////////////////////////////////////

//// Component Mappings 

// There is no function of any other component mapping
constexpr VkComponentMapping COMPONENT_MAPPING_DEFAULT {
    .r = VK_COMPONENT_SWIZZLE_R,
    .g = VK_COMPONENT_SWIZZLE_G,
    .b = VK_COMPONENT_SWIZZLE_B,
    .a = VK_COMPONENT_SWIZZLE_A,
};


//// Image Subresource Ranges

// Spans one layer and one mipmap of the color section
constexpr VkImageSubresourceRange IMAGE_SUBRESOURCE_RANGE_DEFAULT {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .baseMipLevel = 0,
    .levelCount = 1,
    .baseArrayLayer = 0,
    .layerCount = 1,
};

constexpr VkImageSubresourceRange IMAGE_SUBRESOURCE_RANGE_ALL {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .baseMipLevel = 0,
    .levelCount = VK_REMAINING_MIP_LEVELS,
    .baseArrayLayer = 0,
    .layerCount = VK_REMAINING_ARRAY_LAYERS,
};

constexpr VkImageSubresourceRange IMAGE_SUBRESOURCE_RANGE_DEPTH {
    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
    .baseMipLevel = 0,
    .levelCount = 1,
    .baseArrayLayer = 0,
    .layerCount = 1,
};


//// Image Subresource Layers

constexpr VkImageSubresourceLayers IMAGE_SUBRESOURCE_LAYERS_DEFAULT {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .mipLevel = 0,
    .baseArrayLayer = 0,
    .layerCount = 1,
};

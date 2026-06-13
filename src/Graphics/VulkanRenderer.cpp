#include <array>
#include <format>
#include <fstream>
#include <stdexcept>
#include <volk.h>

#include <iostream>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>
#include "gpu_structs.h"
#include "utils.h"
#include "../errors.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include "VulkanRenderer.h"

#include <stb_image.h>

VulkanRenderer::VulkanRenderer(GLFWwindow &window) 
    : m_window(window)
{}

VulkanRenderer::~VulkanRenderer() {

}

void VulkanRenderer::Initialize() {
    InitVulkanInstance();
    InitVulkanDevice();
    CreateSwapChain(m_props.window_extent);
    InitFrameData();
    InitDefaults();
}

void VulkanRenderer::ShutDown() {
    DestroyDefaults();
    DestroyFrameData();
    DestroySwapChain();
    DestroyVulkanDevice();
    DestroyVulkanInstance();
}

std::optional<Frame> VulkanRenderer::BeginFrame() {
    Assert(m_current_frame_index < m_swapchain_images.size(), "Invalid swapchain index!", __FILE__, __LINE__);
    
    if (m_swapchain_resize_requested) {
        ResizeSwapChain();
        return std::nullopt;
    }

    const PerFrameData &current_frame = m_frame_data[m_current_frame_index];
    const VkCommandBuffer &cmd = current_frame.graphics_command_buffer;

    // TODO: HANDLE RESIZE OF SWAPCHAIN

    // Wait for `render_fence`, which was signaled by previous frames queue submission.
    // TODO: Remove magic number
    vkWaitForFences(m_device, 1, &current_frame.render_fence, VK_TRUE, 1'000'000'000);
    vkResetFences(m_device, 1, &current_frame.render_fence);

    vkResetCommandPool(m_device, current_frame.graphics_command_pool, 0);

    // Acquire image then signal `swapchain_acquire_semaphore`
    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(
        m_device,
        m_swapchain,
        1'000'000'000,
        current_frame.swapchain_acquire_semaphore,
        VK_NULL_HANDLE,
        &image_index
    );


    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Surface is no longer compatible with the swapchain. Thus we must recreate it.
        
        m_swapchain_resize_requested = true;
        return std::nullopt;
    }

    if (result != VK_SUBOPTIMAL_KHR)
        VK_CHECK(result);

    m_swapchain_image_index = image_index;

    VkCommandBufferBeginInfo command_buffer_begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(current_frame.graphics_command_buffer, &command_buffer_begin_info);

    return Frame{
        .cmd = cmd,
        .image = current_frame.draw_image,
        .image_view = current_frame.draw_image_view,
        .extent = m_swapchain_extent
    };
}

void VulkanRenderer::EndFrame() {
    const PerFrameData &current_frame = m_frame_data[m_current_frame_index];

    TransitionImageLayout(current_frame.graphics_command_buffer, current_frame.draw_image, IMAGE_SUBRESOURCE_RANGE_DEFAULT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    TransitionImageLayout(current_frame.graphics_command_buffer, m_swapchain_images[m_swapchain_image_index], IMAGE_SUBRESOURCE_RANGE_DEFAULT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    //// Copy image to swapchain ////
    {
        VkImageCopy copy_region {
            .srcSubresource = IMAGE_SUBRESOURCE_LAYERS_DEFAULT,
            .srcOffset = { 0, 0, 0 },
            .dstSubresource = IMAGE_SUBRESOURCE_LAYERS_DEFAULT,
            .dstOffset = { 0, 0, 0 },
            .extent = {
                .width = m_swapchain_extent.width,
                .height = m_swapchain_extent.height,
                .depth = 1,
            },
        };

        vkCmdCopyImage(
            current_frame.graphics_command_buffer,
            current_frame.draw_image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_swapchain_images[m_swapchain_image_index],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copy_region
        );
    }

    // Transition format of swapchain image to prepare for presentation
    TransitionImageLayout(current_frame.graphics_command_buffer, m_swapchain_images[m_swapchain_image_index], IMAGE_SUBRESOURCE_RANGE_DEFAULT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(current_frame.graphics_command_buffer);

    //// Submit and present ////

    std::vector<VkPipelineStageFlags> wait_stages {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    // Wait on `swapchain_acquire_semaphore` and signal `draw_complete_semaphore` and the fence
    VkSubmitInfo submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &current_frame.swapchain_acquire_semaphore,
        .pWaitDstStageMask = wait_stages.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &current_frame.graphics_command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &current_frame.draw_complete_semaphore,
    };
    VK_CHECK(vkQueueSubmit(m_graphics_queue, 1, &submit_info, current_frame.render_fence));

    // Wait on `draw_complete_semaphore` and present
    VkPresentInfoKHR present_info {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &current_frame.draw_complete_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &m_swapchain_image_index
    };

    VkResult result = vkQueuePresentKHR(m_graphics_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // TODO: resize of swapchain needed
        m_swapchain_resize_requested = true;
    } else {
        VK_CHECK(result);
    }

    m_current_frame_index = (m_current_frame_index + 1) % m_swapchain_images.size();
}

void VulkanRenderer::ImmediateSubmit(std::function<void(VkCommandBuffer)> &&callback) const {
    // TODO: Support other queue types

    VK_CHECK(vkResetFences(m_device, 1, &m_graphics_fence));
    VK_CHECK(vkResetCommandBuffer(m_graphics_command_buffer, 0));

    VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(m_graphics_command_buffer, &begin_info));
    callback(m_graphics_command_buffer);
    VK_CHECK(vkEndCommandBuffer(m_graphics_command_buffer));

    VkSubmitInfo submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &m_graphics_command_buffer,
    };
    VK_CHECK(vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_graphics_fence));
    VK_CHECK(vkWaitForFences(m_device, 1, &m_graphics_fence, VK_TRUE, 1'000'000'000));
}

void VulkanRenderer::WriteDescriptorBuffer(uint32_t binding, VkDescriptorSet descriptor_set, VkDescriptorBufferInfo descriptor_info, VkDescriptorType type) {
    VkWriteDescriptorSet write_descriptor_set {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = type,
        .pBufferInfo = &descriptor_info,
    };

    vkUpdateDescriptorSets(m_device, 1, &write_descriptor_set, 0, nullptr);
}

void VulkanRenderer::WriteDescriptorImage(uint32_t binding, VkDescriptorSet descriptor_set, VkDescriptorImageInfo descriptor_info, VkDescriptorType type) {
    VkWriteDescriptorSet write_descriptor_set {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = &descriptor_info,
    };

    vkUpdateDescriptorSets(m_device, 1, &write_descriptor_set, 0, nullptr);
}

VkShaderModule VulkanRenderer::LoadShader(const std::string &file_path) const {
    std::ifstream file(file_path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to load shader at path '" + file_path + "'.");
    }

    size_t file_size = static_cast<size_t>(file.tellg());

    std::cout << "FILE PATH: " << file_path << "\n";
    std::cout << "FILE SIZE: " << file_size << "\n";
    if (file_size % 4 != 0)
        std::cerr << "Shadeer file not byte-aligned. Are you shure this is in SPIR-V format?\n";

    std::vector<char> source_buffer(file_size);

    file.seekg(0);
    file.read(source_buffer.data(), file_size);
    file.close();
    
    VkShaderModuleCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = source_buffer.size(),
        .pCode = reinterpret_cast<const uint32_t *>(source_buffer.data())
    };
    
    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module));
    return shader_module;
}

GPUImage VulkanRenderer::LoadImage2D(const std::string &file_path, VkImageLayout dest_layout) const {
    //// Load data from file ////
    int width = 0;
    int height = 0;
    int num_channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(file_path.data(), &width, &height, &num_channels, STBI_rgb_alpha);
    stbi_set_flip_vertically_on_load(false);
    if (!data) {
        throw std::runtime_error(std::format("Failed to load image at {}: {}", file_path, stbi_failure_reason()));
    }
    const int image_size = width * height * 4;

    std::vector<uint8_t> image_data;
    image_data.assign(data, data + image_size);
    stbi_image_free(data);

    /// Create GPU Structures ////
    GPUImage result {
        .extent = VkExtent3D{
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .depth = 1,
        },
        .format = VK_FORMAT_R8G8B8A8_UNORM,
    };

    VkImageCreateInfo image_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = result.format,
        .extent = result.extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo allocation_create_info {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };
    VK_CHECK(vmaCreateImage(m_allocator, &image_create_info, &allocation_create_info, &result.image, &result.allocation, nullptr));

    VkImageViewCreateInfo image_view_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = result.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = result.format,
        .components = COMPONENT_MAPPING_DEFAULT,
        .subresourceRange = IMAGE_SUBRESOURCE_RANGE_ALL,
    };
    VK_CHECK(vkCreateImageView(m_device, &image_view_create_info, nullptr, &result.image_view));

    //// Upload to GPU ////
    LoadImageData(result, image_data.data(), dest_layout);
    return result;
}

GPUImage VulkanRenderer::LoadImageArray2D(const std::vector<std::string> &file_paths, VkImageLayout dest_layout) const {
    int num_layers = file_paths.size();
    std::vector<std::vector<uint8_t>> image_data {};
    image_data.resize(num_layers);

    //// Load data from file ////
    int width = 0;
    int height = 0;
    int num_channels = 0;
    for (uint32_t i = 0; i < num_layers; ++i) {
        int temp_width = 0;
        int temp_height = 0;
        int temp_num_channels = 0;
        stbi_set_flip_vertically_on_load(true);
        unsigned char *data = stbi_load(file_paths[i].data(), &temp_width, &temp_height, &temp_num_channels, STBI_rgb_alpha);
        stbi_set_flip_vertically_on_load(false);
        if (!data)
            throw std::runtime_error(std::format("Failed to load image at {}: {}", file_paths[i], stbi_failure_reason()));

        if (i == 0) {
            width = temp_width;
            height = temp_height;
            num_channels = temp_num_channels;
        } else {
            Assert(width == temp_width && height == temp_height, "Mismatching dimensions loaded from Image Array!");
            Assert(num_channels == temp_num_channels, "Mismatching format loaded from Image Array!");
        }
        
        const int image_size = width * height * 4;
        image_data[i].assign(data, data + image_size);
        stbi_image_free(data);
    }

    /// Create GPU Structures ////
    GPUImage result {
        .extent = VkExtent3D{
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .depth = 1,
        },
        .format = VK_FORMAT_R8G8B8A8_UNORM,
    };

    VkImageCreateInfo image_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = result.format,
        .extent = result.extent,
        .mipLevels = 1,
        .arrayLayers = static_cast<uint32_t>(num_layers),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo allocation_create_info {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };
    VK_CHECK(vmaCreateImage(m_allocator, &image_create_info, &allocation_create_info, &result.image, &result.allocation, nullptr));

    VkImageViewCreateInfo image_view_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = result.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = result.format,
        .components = COMPONENT_MAPPING_DEFAULT,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = static_cast<uint32_t>(num_layers),
        },
    };
    VK_CHECK(vkCreateImageView(m_device, &image_view_create_info, nullptr, &result.image_view));

    //// Upload to GPU ////
    for (uint32_t i = 0; i < num_layers; ++i) {
        LoadImageData(result, image_data[i].data(), dest_layout, i);
    }
    return result;
}


void VulkanRenderer::DestroyGPUMesh(const GPUMesh &mesh) const {
    vmaDestroyBuffer(m_allocator, mesh.vertex_buffer, mesh.vertex_buffer_alloc);
    vmaDestroyBuffer(m_allocator, mesh.index_buffer, mesh.index_buffer_alloc);
}

void VulkanRenderer::LoadImageData(const GPUImage &image, void *data, VkImageLayout dst_layout, uint32_t layer) const {
    // TODO: Also add option to generate mipmaps
    if (image.image == VK_NULL_HANDLE)
        throw std::runtime_error("Cannot load data into a null image.");

    // Assumes 4-channel image
    const size_t data_size = image.extent.width * image.extent.height * image.extent.depth * 4;

    // Create staging buffer
    VkBuffer staging_buffer;
    VmaAllocation staging_buffer_alloc;
    VmaAllocationInfo staging_allocation_info;
    {
        VkBufferCreateInfo buffer_create_info {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = data_size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VmaAllocationCreateInfo allocation_create_info {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };

        VK_CHECK(vmaCreateBuffer(m_allocator, &buffer_create_info, &allocation_create_info, &staging_buffer, &staging_buffer_alloc, &staging_allocation_info));
    }

    // Copy raw data into this buffer
    std::memcpy(staging_allocation_info.pMappedData, data, data_size);
    ImmediateSubmit([&](VkCommandBuffer cmd) {
        TransitionImageLayout(cmd, image.image, IMAGE_SUBRESOURCE_RANGE_ALL, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy copy_region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = layer,
                .layerCount = 1,
            },
            .imageExtent = image.extent,
        };

        vkCmdCopyBufferToImage(cmd, staging_buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
        if (dst_layout != VK_IMAGE_LAYOUT_UNDEFINED) {
            TransitionImageLayout(cmd, image.image, IMAGE_SUBRESOURCE_RANGE_ALL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dst_layout);
        }
    });
    vmaDestroyBuffer(m_allocator, staging_buffer, staging_buffer_alloc);
}

///////////////////////
/// PRIVATE METHODS ///
///////////////////////

void VulkanRenderer::InitVulkanInstance() {
    VK_CHECK(volkInitialize());

    //// Initialize Instance ////

    std::vector<std::string> requested_extensions {};
    std::vector<std::string> requested_layers {};

    if (m_props.enable_validation) {
        requested_layers.push_back("VK_LAYER_KHRONOS_validation");
        requested_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Add GLFW-needed extensions
    {
        uint32_t count;
        const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
        for (uint32_t i = 0; i < count; ++i) {
            requested_extensions.push_back(glfw_extensions[i]);
        }
    }

    VkApplicationInfo application_info {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Tiny Minecraft",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = m_props.vulkan_version
    };

    // Add those requested extensions that are supported
    std::vector<const char *> extensions;
    {
        uint32_t extension_count = 0;
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr));
        Assert(extension_count > 0, "There should be more than one available instance extension!", __FILE__, __LINE__);

        std::vector<VkExtensionProperties> supported_extensions(extension_count);
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, supported_extensions.data()));

        const auto add_if_supported = [&](const char *extension_name) {
            for (auto &extension : supported_extensions) {
                if (std::strcmp(extension.extensionName, extension_name) == 0) {
                    extensions.push_back(extension_name);
                    return;
                }
            }

            std::cerr << "Required instance extension unvailable: " << extension_name << std::endl;
        };

        for (auto &ext : requested_extensions) {
            add_if_supported(ext.c_str());
        }
    }

    // Add those requested layers that are supported
    std::vector<const char *> layers;
    {
        uint32_t layer_count = 0;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_count, nullptr));
        Assert(layer_count > 0, "There should be more than one available instance layer!", __FILE__, __LINE__);

        std::vector<VkLayerProperties> supported_layers(layer_count);
        VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_count, supported_layers.data()));

        const auto add_if_supported = [&](const char *layer_name) {
            for (auto &layer : supported_layers) {
                if (std::strcmp(layer.layerName, layer_name) == 0) {
                    layers.push_back(layer_name);
                    return;
                }
            }

            std::cerr << "Required instance layer unvailable: " << layer_name << std::endl;
        };

        for (auto &layer : requested_layers) {
            add_if_supported(layer.c_str());
        }
    }

    VkInstanceCreateInfo instance_create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &application_info,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    std::cout << "Enabling instance extensions: " << std::endl;
    for (const auto &extension_name : extensions) {
        std::cout << " - " << extension_name << std::endl;
    }
    std::cout << "Enabling layers: " << std::endl;
    for (const auto &layer_name : layers) {
        std::cout << " - " << layer_name << std::endl;
    }

    //// Prepare Debug Messenger ////

    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info {};
    if (m_props.enable_validation) {
        debug_messenger_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = VulkanRenderer::DebugCallback,
            .pUserData = nullptr,
        };

        instance_create_info.pNext = &debug_messenger_create_info;
    }

    //// Create Instance ////

    VK_CHECK(vkCreateInstance(&instance_create_info, nullptr, &m_instance));

    volkLoadInstance(m_instance);
    if (m_props.enable_validation) {
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_instance, &debug_messenger_create_info, nullptr, &m_debug_messenger))
    }

    //// Initialize Window Surface ////

    glfwCreateWindowSurface(m_instance, &m_window, nullptr, &m_surface);
}

void VulkanRenderer::DestroyVulkanInstance() {
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

    if (m_props.enable_validation) {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
    }
    
    vkDestroyInstance(m_instance, nullptr);

    std::cout << " - Destroyed Vulkan Instance\n";
}

void VulkanRenderer::InitVulkanDevice() {
    if (!SupportsApiVersion(m_props.vulkan_version, VK_MAKE_API_VERSION(0, 1, 3, 0))) {
        throw std::runtime_error("We require support for Vulkan 1.3.0 or greater!\n");
    }

    std::vector<std::string> requested_extensions;
    requested_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    //// Choose Phyiscal Device ////

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
    Assert(device_count > 0, "There must be at least one device!", __FILE__, __LINE__);

    std::vector<VkPhysicalDevice> physical_devices(device_count);
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &device_count, physical_devices.data()));

    std::cout << "Available devices: " << std::endl;
    for (auto device : physical_devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        std::cout << " - " << properties.deviceName << std::endl;
    }

    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    int best_score = -1;

    // Very simple selection algorithm to try to bias discrete GPUs
    uint32_t device_group_count;
    vkEnumeratePhysicalDeviceGroups(m_instance, &device_group_count, nullptr);

    std::vector<VkPhysicalDeviceGroupProperties> device_groups(device_group_count);
    vkEnumeratePhysicalDeviceGroups(m_instance, &device_group_count, device_groups.data());

    for (const auto &group : device_groups) {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(group.physicalDevices[0], &properties);
        vkGetPhysicalDeviceFeatures(group.physicalDevices[0], &features);

        int score = 0;
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 100;
        }

        score += static_cast<int>(properties.limits.maxImageDimension2D);

        if (score > best_score) {
            best_score = score;
            best_device = group.physicalDevices[0];
        }
    }

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(best_device, &device_properties);
    std::cout << "Chose physical device: " << device_properties.deviceName << std::endl;

    m_physical_device = best_device;

    //// Create Software Device ////

    // TODO: Also choose a compute queue and possibly a transfer queue
    // Choose graphics queue
    std::vector<VkDeviceQueueCreateInfo> queue_infos;

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
        Assert(queue_family_count > 0, "There must be at least one queue family for this device!", __FILE__, __LINE__);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, queue_families.data());
    
        // Find the first queue family with both graphics and present support
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 present_support = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, m_surface, &present_support);

                if (present_support) {
                    m_graphics_queue_family = i;
                    break;
                }
            }
        }

        Assert(m_graphics_queue_family != std::numeric_limits<uint32_t>::max(), "Cannot find a queue with both present and graphics capabilities.", __FILE__, __LINE__);

        queue_create_info.queueFamilyIndex = m_graphics_queue_family;
        queue_infos.emplace_back(queue_create_info);
    }

    // Adding requested device extensions if supported
    std::vector<const char *> extensions {};
    {
        uint32_t extension_count = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr, &extension_count, nullptr));
        
        std::vector<VkExtensionProperties> supported_extensions(extension_count);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr, &extension_count, supported_extensions.data()));
    
        const auto add_if_supported = [&](const char *extension_name) {
            for (auto extension : supported_extensions) {
                if (std::strcmp(extension.extensionName, extension_name) == 0) {
                    extensions.push_back(extension_name);
                    return;
                }
            }

            throw std::runtime_error(std::format("Required extension unavailable: {}", extension_name));
        };
        
        for (auto &ext : requested_extensions) {
            add_if_supported(ext.c_str());
        }

        std::cout << "Enabling device extensions: " << std::endl;
        for (const auto &extension_name : extensions) {
            std::cout << " - " << extension_name << std::endl;
        }
    }

    // Enabling Vulkan features
    std::vector<VkBaseOutStructure *> feature_chain;
    VkPhysicalDeviceVulkan12Features feat12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE
    };

    VkPhysicalDeviceVulkan13Features feat13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = nullptr,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    feature_chain.push_back(reinterpret_cast<VkBaseOutStructure *>(&feat12));
    feature_chain.push_back(reinterpret_cast<VkBaseOutStructure *>(&feat13));

    // Connect feature linked list
    for (size_t i = 1; i < feature_chain.size(); ++i) {
        feature_chain[i - 1]->pNext = feature_chain[i];
    }

    VkDeviceCreateInfo device_create_info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = feature_chain.empty() ? nullptr : feature_chain.front(),
        .queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size()),
        .pQueueCreateInfos = queue_infos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
        .pEnabledFeatures = nullptr,
    };

    VK_CHECK(vkCreateDevice(m_physical_device, &device_create_info, nullptr, &m_device));

    volkLoadDevice(m_device);
    vkGetDeviceQueue(m_device, m_graphics_queue_family, 0, &m_graphics_queue);

    //// Set up Vulkan Memory Allocator ////
    VmaVulkanFunctions vulkan_functions = {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };

    VmaAllocatorCreateInfo allocatorInfo = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = m_physical_device,
        .device = m_device,
        .pVulkanFunctions = &vulkan_functions,
        .instance = m_instance,
    };
    vmaCreateAllocator(&allocatorInfo, &m_allocator);

    //// Pick Supported Formats ////

    const auto pick_supported_format = [&](const std::vector<VkFormat> &candidates, VkFormatFeatureFlags feature) {
        for (VkFormat format : candidates) {
            VkFormatProperties properties;
            vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &properties);
            if (properties.optimalTilingFeatures & feature) {
                return format;
            }
        }
        return VK_FORMAT_UNDEFINED;
    };

    {
        /// --- COLOR //
        // ordered from most preferable
        const std::vector<VkFormat> color_candidates = {
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R8G8B8A8_SRGB
        };
        m_image_formats.color = pick_supported_format(color_candidates, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
        Assert(m_image_formats.color != VK_FORMAT_UNDEFINED, "Cannot find valid color format");

        /// --- DEPTH STENCIL //
        const std::vector<VkFormat> depth_stencil_candidates = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT
        };
        m_image_formats.depth_stencil = pick_supported_format(depth_stencil_candidates, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        Assert(m_image_formats.depth_stencil != VK_FORMAT_UNDEFINED, "Cannot find valid depth-stencil format");

        /// --- DEPTH ONLY //
        const std::vector<VkFormat> depth_candidates = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D16_UNORM,
        };
        m_image_formats.depth = pick_supported_format(depth_candidates, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        Assert(m_image_formats.depth != VK_FORMAT_UNDEFINED, "Cannot find valid depth format");

        /// --- HDR COLOR //
        const std::vector<VkFormat> hdr_color_candidates = {
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_A2B10G10R10_UNORM_PACK32,
            VK_FORMAT_R8G8B8A8_SRGB // not a valid HDR format, but will do as fallback
        };
        m_image_formats.hdr = pick_supported_format(hdr_color_candidates, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
        Assert(m_image_formats.hdr != VK_FORMAT_UNDEFINED, "Cannot find valid HDR format");
    }

    //// Create immediate command buffers and command pools ////
    {
        VkCommandPoolCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_graphics_queue_family,
        };
        VK_CHECK(vkCreateCommandPool(m_device, &create_info, nullptr, &m_graphics_command_pool));
    
        VkCommandBufferAllocateInfo command_buffer_allocate_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_graphics_command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_CHECK(vkAllocateCommandBuffers(m_device, &command_buffer_allocate_info, &m_graphics_command_buffer));
    }

    VkFenceCreateInfo fence_create_info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    VK_CHECK(vkCreateFence(m_device, &fence_create_info, nullptr, &m_graphics_fence));
}

void VulkanRenderer::DestroyVulkanDevice() {
    vkDestroyFence(m_device, m_graphics_fence, nullptr);

    vkFreeCommandBuffers(m_device, m_graphics_command_pool, 1, &m_graphics_command_buffer);
    vkDestroyCommandPool(m_device, m_graphics_command_pool, nullptr);

    vmaDestroyAllocator(m_allocator);
    vkDestroyDevice(m_device, nullptr);

    std::cout << " - Destroyed Vulkan Device\n";
}

void VulkanRenderer::CreateSwapChain(const VkExtent2D &extent) {
    VkSwapchainKHR old_swapchain = m_swapchain;

    VkSurfaceCapabilitiesKHR surface_capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &surface_capabilities));

    // Choose present format
    VkSurfaceFormatKHR swapchain_format;
    {
        uint32_t surface_formats_count = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &surface_formats_count, nullptr));
        Assert(surface_formats_count > 0, "Cannot find any surface formats.", __FILE__, __LINE__);

        std::vector<VkSurfaceFormatKHR> surface_formats(surface_formats_count);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &surface_formats_count, surface_formats.data()));
    
        bool found = false;
        for (const auto &format : surface_formats) {
            if (format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                swapchain_format = format;
                found = true;
                break;
            }
        }
        
        if (!found) {
            swapchain_format = surface_formats[0];
        }
    }

    // Choose present mode
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    {
        uint32_t present_modes_count = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_modes_count, nullptr));
        Assert(present_modes_count > 0, "Cannot find any present modes.", __FILE__, __LINE__);

    
        std::vector<VkPresentModeKHR> present_modes(present_modes_count);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_modes_count, present_modes.data()));
    
        // if (m_props.enable_vsync) {
            for (const auto &mode : present_modes) {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }

                if (mode == VK_PRESENT_MODE_FIFO_KHR) {
                    present_mode = VK_PRESENT_MODE_FIFO_KHR;
                }
            }
        // }
    }

    // Choose extent
    m_swapchain_extent = surface_capabilities.currentExtent;
    if (surface_capabilities.currentExtent.width == UINT32_MAX) {
        m_swapchain_extent = extent;
        m_swapchain_extent.width = std::clamp(m_swapchain_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
        m_swapchain_extent.height = std::clamp(m_swapchain_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    }

    // Choose image count
    uint32_t image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount) {
        image_count = surface_capabilities.maxImageCount;
    }

    // Choose pre-transform
    VkSurfaceTransformFlagBitsKHR pre_transform;
    if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }

    // Choose swapchain blending mode
    VkCompositeAlphaFlagBitsKHR composite_alpha;
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
        .surface = m_surface,
        .minImageCount = image_count,
        .imageFormat = swapchain_format.format,
        .imageColorSpace = swapchain_format.colorSpace,
        .imageExtent = m_swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = pre_transform,
        .compositeAlpha = composite_alpha,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain,
    };

    Assert(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "Surface does not support TRANSFER_SRC", __FILE__, __LINE__);
    Assert(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT, "Surface does not support TRANSFER_DST", __FILE__, __LINE__);

    swapchain_create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapchain_create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VK_CHECK(vkCreateSwapchainKHR(m_device, &swapchain_create_info, nullptr, &m_swapchain));

    if (old_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, old_swapchain, nullptr);
    }

    //// Get Swapchain Images ////
    VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr));

    m_swapchain_images.resize(image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_swapchain_images.data()));
}

void VulkanRenderer::DestroySwapChain() {
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    std::cout << " - Destroyed swap chain.\n";
}

void VulkanRenderer::InitFrameData() {
    m_frame_data.resize(m_swapchain_images.size());

    VkSemaphoreCreateInfo semaphore_create_info {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
    };

    VkFenceCreateInfo fence_create_info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (int i = 0; i < m_swapchain_images.size(); ++i) {
        VK_CHECK(vkCreateSemaphore(m_device, &semaphore_create_info, nullptr, &m_frame_data[i].draw_complete_semaphore));
        VK_CHECK(vkCreateSemaphore(m_device, &semaphore_create_info, nullptr, &m_frame_data[i].swapchain_acquire_semaphore));
        VK_CHECK(vkCreateFence(m_device, &fence_create_info, nullptr, &m_frame_data[i].render_fence));
    
        // Create graphics command pool and buffer
        VkCommandPoolCreateInfo command_pool_create_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = m_graphics_queue_family
        };
        VK_CHECK(vkCreateCommandPool(m_device, &command_pool_create_info, nullptr, &m_frame_data[i].graphics_command_pool));
    
        VkCommandBufferAllocateInfo command_buffer_allocate_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_frame_data[i].graphics_command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_CHECK(vkAllocateCommandBuffers(m_device, &command_buffer_allocate_info, &m_frame_data[i].graphics_command_buffer));
    
        // Create draw image and image view
        {
            VkImageCreateInfo image_info {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .flags = 0,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = m_image_formats.color,
                .extent = {
                    .width = m_swapchain_extent.width,
                    .height = m_swapchain_extent.height,
                    .depth = 1,
                },
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            };

            VmaAllocationCreateInfo allocation_info {
                .flags = 0,
                .usage = VMA_MEMORY_USAGE_AUTO,
            };
            vmaCreateImage(
                m_allocator,
                &image_info,
                &allocation_info,
                &m_frame_data[i].draw_image,
                &m_frame_data[i].draw_image_allocation,
                nullptr);

            VkImageViewCreateInfo image_view_create_info {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = m_frame_data[i].draw_image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = m_image_formats.color,
                .components = COMPONENT_MAPPING_DEFAULT,
                .subresourceRange = IMAGE_SUBRESOURCE_RANGE_DEFAULT,
            };
            vkCreateImageView(m_device, &image_view_create_info, nullptr, &m_frame_data[i].draw_image_view);
        }
    }
}

void VulkanRenderer::ResizeSwapChain() {
    vkDeviceWaitIdle(m_device);

    int width, height;
    glfwGetWindowSize(&m_window, &width, &height);
    m_props.window_extent.width = width;
    m_props.window_extent.height = height;

    m_swapchain_resize_requested = false;

    DestroyFrameData();
    CreateSwapChain(m_props.window_extent);
    InitFrameData();
    m_current_frame_index = 0;
}

void VulkanRenderer::DestroyFrameData() {
    for (int i = 0; i < m_swapchain_images.size(); ++i) {
        vkDestroySemaphore(m_device, m_frame_data[i].draw_complete_semaphore, nullptr);    
        vkDestroySemaphore(m_device, m_frame_data[i].swapchain_acquire_semaphore, nullptr);
        vkDestroyFence(m_device, m_frame_data[i].render_fence, nullptr);

        vkFreeCommandBuffers(m_device, m_frame_data[i].graphics_command_pool, 1, &m_frame_data[i].graphics_command_buffer);
        vkDestroyCommandPool(m_device, m_frame_data[i].graphics_command_pool, nullptr);

        vkDestroyImageView(m_device, m_frame_data[i].draw_image_view, nullptr);
        vmaDestroyImage(m_allocator, m_frame_data[i].draw_image, m_frame_data[i].draw_image_allocation);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    std::string prefix;
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        prefix = "VERBOSE";
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        prefix = "INFO";
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        prefix = "WARNING";
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        prefix = "ERROR";

    // if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    //     throw std::runtime_error(std::format("Validation Message [{}]: {}", prefix.c_str(), callback_data->pMessage));
    // } else {
    fprintf(stderr, "Validation Message [%s]: {%s}", prefix.c_str(), callback_data->pMessage);
    // }

    return VK_FALSE;
}

void VulkanRenderer::InitDefaults() {
    //// ---- Default Samplers ---- ////
    VkSamplerCreateInfo nearest_sampler_create_info {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
    };

    VkSamplerCreateInfo linear_sampler_create_info {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
    };
    VK_CHECK(vkCreateSampler(m_device, &nearest_sampler_create_info, nullptr, &m_default_samplers.nearest));
    VK_CHECK(vkCreateSampler(m_device, &linear_sampler_create_info, nullptr, &m_default_samplers.linear));

    uint32_t gray = glm::packUnorm4x8(glm::vec4(0.65f, 0.65f, 0.65f, 1.0f));
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    uint32_t white = glm::packUnorm4x8(glm::vec4(0));
    uint32_t black = glm::packUnorm4x8(glm::vec4(0));

    // Default black material
    {
        m_default_textures.white = CreateDefaultImage(VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM);
        LoadImageData(*m_default_textures.white, (void *) &white, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // Default gray material
    {
        m_default_textures.gray = CreateDefaultImage(VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM);
        LoadImageData(*m_default_textures.gray, (void *) &gray, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // Default checker material
    {
        m_default_textures.checker = CreateDefaultImage(VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM);
        std::array<uint32_t, 16 * 16> checker_pixels; //for 16x16 checkerboard texture
        for (int x = 0; x < 16; ++x) {
            for (int y = 0; y < 16; ++y) {
                checker_pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
            }
        }
        LoadImageData(*m_default_textures.checker, checker_pixels.data(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void VulkanRenderer::DestroyDefaults() {
    vkDestroySampler(m_device, m_default_samplers.nearest, nullptr);
    vkDestroySampler(m_device, m_default_samplers.linear, nullptr);
    
    vkDestroyImageView(m_device, m_default_textures.white->image_view, nullptr);
    vmaDestroyImage(m_allocator, m_default_textures.white->image, m_default_textures.white->allocation);

    vkDestroyImageView(m_device, m_default_textures.gray->image_view, nullptr);
    vmaDestroyImage(m_allocator, m_default_textures.gray->image, m_default_textures.gray->allocation);

    vkDestroyImageView(m_device, m_default_textures.checker->image_view, nullptr);
    vmaDestroyImage(m_allocator, m_default_textures.checker->image, m_default_textures.checker->allocation);
}

std::shared_ptr<GPUImage> VulkanRenderer::CreateDefaultImage(VkExtent3D extent, VkFormat format) {
    GPUImage image {
        .extent = extent,
        .format = format,
    };
    
    VkImageCreateInfo image_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VmaAllocationCreateInfo allocation_create_info {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    vmaCreateImage(m_allocator, &image_create_info, &allocation_create_info, &image.image, &image.allocation, nullptr);

    VkImageViewCreateInfo image_view_create_info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = image.format,
        .components = COMPONENT_MAPPING_DEFAULT,
        .subresourceRange = IMAGE_SUBRESOURCE_RANGE_ALL,
    };
    vkCreateImageView(m_device, &image_view_create_info, nullptr, &image.image_view);
    return std::make_shared<GPUImage>(image);
}

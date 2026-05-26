#include <volk.h>

#include <iostream>
#include <string>
#include <vector>
#include "utils/vulkan.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "vulkan_renderer.h"

VulkanRenderer::VulkanRenderer() {

}

VulkanRenderer::~VulkanRenderer() {

}

void VulkanRenderer::Initialize(GLFWwindow *window) {
    InitVulkanInstance(window);
    InitVulkanDevice();
}

void VulkanRenderer::InitVulkanInstance(GLFWwindow *window) {
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
        // TODO: Assert > 0

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
        // TODO: Assert > 0

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

    glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface);
}

void VulkanRenderer::InitVulkanDevice() {
    if (!SupportsApiVersion(m_props.vulkan_version, VK_MAKE_API_VERSION(0, 1, 3, 0))) {
        std::cerr << "We require support for Vulkan 1.3.0 or greater. Please update your drivers to continue!\n";
        exit(1); // TODO: Better error handling
    }
    std::vector<std::string> requested_extensions;

    requested_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    //// Choose Phyiscal Device ////

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
    // TODO: Assert > 0

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
        // TODO: Assert > 0

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

        if (m_graphics_queue_family == std::numeric_limits<uint32_t>::max()) { 
            std::cerr << "Cannot find a queue with both present and graphics capabilities" << std::endl;
            // TODO: Error handling
        }

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

            std::cerr << "Required extension unavailable: " << extension_name << "\n";
            // TODO: Error handling
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

    const std::vector<VkFormat> color_candidates = {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB
    };
    m_image_formats.color = pick_supported_format(color_candidates, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
    if (m_image_formats.color == VK_FORMAT_UNDEFINED)
        std::cerr << "Cannot find valid color format!\n";
    
    const std::vector<VkFormat> hdr_color_candidates = {
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
    };
    m_image_formats.hdr = pick_supported_format(hdr_color_candidates, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
    if (m_image_formats.hdr == VK_FORMAT_UNDEFINED)
        std::cerr << "Cannot find valid hdr format!\n";
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

    fprintf(stderr, "Validation Message [%s]: {%s}", prefix.c_str(), callback_data->pMessage);

    return VK_FALSE;
}
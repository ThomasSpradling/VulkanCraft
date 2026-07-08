#include "VulkanDevice.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <print>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include <GLFW/glfw3.h>

#include "Core/errors.h"
#include "VulkanObjects.h"

VulkanDevice::VulkanDevice(const Window &window, DeviceConfig config)
    : m_config(config)
{
    CreateVulkanInstance();
    CreateVulkanSurface(window);
    CreateVulkanDevice();
    CreateVulkanMemoryAllocator();
    
    CreateImmediateObjects();

    std::cout << "Created Vulkan Device.\n";    
}

VulkanDevice::~VulkanDevice() {
    DestroyImmediateObjects();

    DestroyVulkanMemoryAllocator();
    DestroyVulkanDevice();
    DestroyVulkanSurface();
    DestroyVulkanInstance();

    std::cout << "Destroyed Vulkan Device.\n";
}

VkQueue VulkanDevice::Queue(QueueType type) const {
    switch (type) {
        case QueueType::Present:
            return m_present_queue.queue;
        case QueueType::Compute:
            return m_compute_queue.queue;
        case QueueType::Transfer:
            return m_transfer_queue.queue;
        case QueueType::DedicatedCompute:
            return m_dedicated_compute_queue.queue;
        case QueueType::Graphics:
        default:
            return m_graphics_queue.queue;
    }
}

uint32_t VulkanDevice::QueueFamily(QueueType type) const {
    switch (type) {
        case QueueType::Present:
            return m_present_queue.queue_family;
        case QueueType::Compute:
            return m_compute_queue.queue_family;
        case QueueType::Transfer:
            return m_transfer_queue.queue_family;
        case QueueType::DedicatedCompute:
            return m_dedicated_compute_queue.queue_family;
        case QueueType::Graphics:
        default:
            return m_graphics_queue.queue_family;
    }
}

void VulkanDevice::ImmediateSubmit(QueueType type, const std::function<void(VkCommandBuffer)> &record, bool async) const {
    if (type == QueueType::Present)
        throw std::runtime_error("Cannot record present operations in a command buffer for immediate submit!");

    if (type == QueueType::Compute)
        type = QueueType::DedicatedCompute;

    const ImmediateContext &context = [&]() -> const ImmediateContext & {
        switch (type) {
            case QueueType::Transfer:
                return m_immediate_transfer;
            case QueueType::DedicatedCompute:
                return m_immediate_compute;
            case QueueType::Graphics:
            default:
                return m_immediate_graphics;
        };
    }();

    VK_CHECK(vkWaitForFences(m_device, 1, &context.fence, VK_TRUE, ImmediateFenceMaxTimeout * 1'000'000));
    VK_CHECK(vkResetCommandPool(m_device, context.command_pool, 0));

    struct VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VK_CHECK(vkBeginCommandBuffer(context.command_buffer, &begin_info));
    record(context.command_buffer);
    VK_CHECK(vkEndCommandBuffer(context.command_buffer));

    VkCommandBufferSubmitInfo command_submit_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = context.command_buffer,
    };

    VkSubmitInfo2 submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &command_submit_info,
    };

    VK_CHECK(vkResetFences(m_device, 1, &context.fence));
    VK_CHECK(vkQueueSubmit2(Queue(type), 1, &submit_info, context.fence));

    if (!async) {
        VK_CHECK(vkWaitForFences(m_device, 1, &context.fence, VK_TRUE, ImmediateFenceMaxTimeout * 1'000'000));
    }
}

void VulkanDevice::QueueSubmit(QueueType type, const QueueSubmitInfo &submit_info, const VulkanFence &fence) {
    // Wait Semaphores
    std::vector<VkSemaphoreSubmitInfo> wait_semaphores;
    wait_semaphores.resize(submit_info.wait_semaphores.size());

    std::ranges::transform(submit_info.wait_semaphores.begin(), submit_info.wait_semaphores.end(), wait_semaphores.begin(), [](const QueueSubmitInfo::SemaphoreSubmit &semaphore) {
        return VkSemaphoreSubmitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = semaphore.semaphore->Handle(),
            .value = semaphore.value,
            .stageMask = semaphore.stage,
        };
    });

    // Signal Semaphores
    std::vector<VkSemaphoreSubmitInfo> signal_semaphores;
    signal_semaphores.resize(submit_info.signal_semaphores.size());

    std::ranges::transform(submit_info.signal_semaphores.begin(), submit_info.signal_semaphores.end(), signal_semaphores.begin(), [](const QueueSubmitInfo::SemaphoreSubmit &semaphore) {
        return VkSemaphoreSubmitInfo {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = semaphore.semaphore->Handle(),
            .value = semaphore.value,
            .stageMask = semaphore.stage,
        };
    });

    // Command Buffers
    std::vector<VkCommandBufferSubmitInfo> command_buffers;
    command_buffers.resize(submit_info.command_buffers.size());

    std::ranges::transform(submit_info.command_buffers.begin(), submit_info.command_buffers.end(), command_buffers.begin(), [](const CommandBuffer *buffer) {
        return VkCommandBufferSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = buffer->Handle(),
        };
    });
    
    VkSubmitInfo2 vk_submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = static_cast<uint32_t>(wait_semaphores.size()),
        .pWaitSemaphoreInfos = wait_semaphores.data(),
        .commandBufferInfoCount = static_cast<uint32_t>(command_buffers.size()),
        .pCommandBufferInfos = command_buffers.data(),
        .signalSemaphoreInfoCount = static_cast<uint32_t>(signal_semaphores.size()),
        .pSignalSemaphoreInfos = signal_semaphores.data(),
    };
    
    VK_CHECK(vkQueueSubmit2(Queue(type), 1, &vk_submit_info, fence.Handle()));
}

std::unique_ptr<ShaderModule> VulkanDevice::CreateShaderModule(const std::vector<uint32_t> &spriv_code) const {
    return std::make_unique<ShaderModule>(*this, spriv_code);
}

std::unique_ptr<VulkanSemaphore> VulkanDevice::CreateBinarySemaphore() const {
    return std::make_unique<VulkanSemaphore>(*this);
}

std::unique_ptr<VulkanSemaphore> VulkanDevice::CreateTimelineSemaphore(uint64_t initial_value) const {
    return std::make_unique<VulkanSemaphore>(*this, SemaphoreType::Timeline, initial_value);
}

std::unique_ptr<VulkanFence> VulkanDevice::CreateFence(bool signalled) const {
    return std::make_unique<VulkanFence>(*this, signalled);
}

std::unique_ptr<VulkanCommandPool> VulkanDevice::CreateCommandPool(QueueType queue, VkCommandPoolCreateFlags flags) const {
    return std::make_unique<VulkanCommandPool>(*this, queue, flags);
}

void VulkanDevice::CreateVulkanInstance() {
    VK_CHECK(volkInitialize());

    //// Initialize Instance ////

    std::vector<std::string> requested_extensions {};
    std::vector<std::string> requested_layers {};

    if (m_config.enable_validation) {
        requested_layers.emplace_back("VK_LAYER_KHRONOS_validation");
        requested_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Add GLFW-needed extensions
    {
        uint32_t count;
        const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
        for (uint32_t i = 0; i < count; ++i) {
            requested_extensions.emplace_back(glfw_extensions[i]);
        }
    }

    VkApplicationInfo application_info {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Tiny Minecraft",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = m_config.vulkan_version,
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
                    extensions.emplace_back(extension_name);
                    return;
                }
            }

            std::cerr << "Requested instance extension unvailable: " << extension_name << "\n";
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
                    layers.emplace_back(layer_name);
                    return;
                }
            }

            std::cerr << "Required instance layer unavailable: " << layer_name << "\n";
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

    std::cout << "Enabling instance extensions: " << "\n";
    for (const auto &extension_name : extensions) {
        std::cout << " - " << extension_name << "\n";
    }
    std::cout << "Enabling layers: " << "\n";
    for (const auto &layer_name : layers) {
        std::cout << " - " << layer_name << "\n";
    }

    //// Prepare Debug Messenger ////

    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info {};

    if (m_config.enable_validation) {
        debug_messenger_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = VulkanDevice::DebugCallback,
            .pUserData = nullptr,
        };

        instance_create_info.pNext = &debug_messenger_create_info;
    }

    //// Create Instance ////

    VK_CHECK(vkCreateInstance(&instance_create_info, nullptr, &m_instance));

    volkLoadInstance(m_instance);
    if (m_config.enable_validation) {
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_instance, &debug_messenger_create_info, nullptr, &m_debug_messenger))
    }
}

void VulkanDevice::DestroyVulkanInstance() {
    if (m_config.enable_validation) {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
    }

    vkDestroyInstance(m_instance, nullptr);
}

void VulkanDevice::CreateVulkanSurface(const Window &window) {
    VK_CHECK(glfwCreateWindowSurface(m_instance, window.GetHandle(), nullptr, &m_surface));
}

void VulkanDevice::DestroyVulkanSurface() {
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
}

void VulkanDevice::CreateVulkanDevice() {
    std::vector<std::string> requested_extensions;
    requested_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    requested_extensions.emplace_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);

    m_physical_device = ChoosePhysicalDevice();

    // Choose graphics queue
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos = ChooseQueues();

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
                    extensions.emplace_back(extension_name);
                    return;
                }
            }

            std::cerr << "Requested extension unavailable: " << extension_name << ".\n";

            if (extension_name == std::string(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
                throw std::runtime_error("  This was a mandatory extension.");
        };

        for (auto &ext : requested_extensions) {
            add_if_supported(ext.c_str());
        }

        std::cout << "Enabling device extensions: " << "\n";
        for (const auto &extension_name : extensions) {
            std::cout << " - " << extension_name << "\n";
        }
    }

    // Enabling Vulkan features
    std::vector<VkBaseOutStructure *> feature_chain;

    VkPhysicalDeviceVulkan11Features feat11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .shaderDrawParameters = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features feat12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features feat13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = nullptr,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
        .maintenance4 = VK_TRUE,
    };

    feature_chain.emplace_back(reinterpret_cast<VkBaseOutStructure *>(&feat11));
    feature_chain.emplace_back(reinterpret_cast<VkBaseOutStructure *>(&feat12));
    feature_chain.emplace_back(reinterpret_cast<VkBaseOutStructure *>(&feat13));

    // Connect feature linked list
    for (size_t i = 1; i < feature_chain.size(); ++i) {
        feature_chain[i - 1]->pNext = feature_chain[i];
    }

    VkDeviceCreateInfo device_create_info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = feature_chain.empty() ? nullptr : feature_chain.front(),
        .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
        .pEnabledFeatures = nullptr,
    };

    VK_CHECK(vkCreateDevice(m_physical_device, &device_create_info, nullptr, &m_device));

    volkLoadDevice(m_device);

    // Get device queues
    vkGetDeviceQueue(m_device, m_graphics_queue.queue_family, 0, &m_graphics_queue.queue);
    vkGetDeviceQueue(m_device, m_compute_queue.queue_family, 0, &m_compute_queue.queue);
    vkGetDeviceQueue(m_device, m_present_queue.queue_family, 0, &m_present_queue.queue);
    vkGetDeviceQueue(m_device, m_transfer_queue.queue_family, 0, &m_transfer_queue.queue);
    vkGetDeviceQueue(m_device, m_dedicated_compute_queue.queue_family, 0, &m_dedicated_compute_queue.queue);

    // Set debug queue names
    SetDebugName(m_graphics_queue.queue, "Graphics Queue");
    SetDebugName(m_transfer_queue.queue, "Transfer Queue");
    SetDebugName(m_compute_queue.queue, "Dedicated Compute Queue");
}

void VulkanDevice::DestroyVulkanDevice() {
    vkDestroyDevice(m_device, nullptr);
}

void VulkanDevice::CreateVulkanMemoryAllocator() {
    VmaVulkanFunctions vulkan_functions = {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };

    VmaAllocatorCreateInfo allocator_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = m_physical_device,
        .device = m_device,
        .pVulkanFunctions = &vulkan_functions,
        .instance = m_instance,
        .vulkanApiVersion = m_config.vulkan_version,
    };
    VK_CHECK(vmaCreateAllocator(&allocator_info, &m_allocator));
}

void VulkanDevice::DestroyVulkanMemoryAllocator() {
    vmaDestroyAllocator(m_allocator);
}

void VulkanDevice::CreateImmediateObjects() {
    auto create_immediate_object = [this](QueueType type, ImmediateContext &context) {
        std::string type_name;
        switch (type) {
            case QueueType::Transfer:
                type_name = "Transfer";
                break;
            case QueueType::Compute:
            case QueueType::DedicatedCompute:
                type_name = "Compute";
                break;
            case QueueType::Graphics:
            default:
                type_name = "Graphics";
                break;
        }

        // Create command pool
        VkCommandPoolCreateInfo pool_create_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = QueueFamily(type),
        };
        VK_CHECK(vkCreateCommandPool(m_device, &pool_create_info, nullptr, &context.command_pool));
        SetDebugName(context.command_pool, std::format("Immediate {} Command Pool", type_name));

        // Allocate command buffer
        VkCommandBufferAllocateInfo allocate_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = context.command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        VK_CHECK(vkAllocateCommandBuffers(m_device, &allocate_info, &context.command_buffer));
        SetDebugName(context.command_buffer, std::format("Immediate {} Command Buffer", type_name));

        // Create fence
        VkFenceCreateInfo fence_create_info {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        VK_CHECK(vkCreateFence(m_device, &fence_create_info, nullptr, &context.fence));
        SetDebugName(context.command_buffer, std::format("Immediate {} Fence", type_name));
    };

    create_immediate_object(QueueType::Graphics, m_immediate_graphics);
    create_immediate_object(QueueType::DedicatedCompute, m_immediate_compute);
    create_immediate_object(QueueType::Transfer, m_immediate_transfer);
}

void VulkanDevice::DestroyImmediateObjects() {
    vkDestroyCommandPool(m_device, m_immediate_graphics.command_pool, nullptr);
    vkDestroyFence(m_device, m_immediate_graphics.fence, nullptr);

    vkDestroyCommandPool(m_device, m_immediate_compute.command_pool, nullptr);
    vkDestroyFence(m_device, m_immediate_compute.fence, nullptr);

    vkDestroyCommandPool(m_device, m_immediate_transfer.command_pool, nullptr);
    vkDestroyFence(m_device, m_immediate_transfer.fence, nullptr);
}

VkPhysicalDevice VulkanDevice::ChoosePhysicalDevice() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
    Assert(device_count > 0, "There must be at least one device!", __FILE__, __LINE__);

    std::vector<VkPhysicalDevice> physical_devices(device_count);
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &device_count, physical_devices.data()));

    std::cout << "Available devices: " << "\n";
    for (auto device : physical_devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        std::cout << " - " << properties.deviceName << "\n";
    }

    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    int best_score = -1;

    // Very simple selection algorithm to try to bias discrete GPUs
    uint32_t device_group_count;
    VK_CHECK(vkEnumeratePhysicalDeviceGroups(m_instance, &device_group_count, nullptr));

    std::vector<VkPhysicalDeviceGroupProperties> device_groups(device_group_count);
    std::ranges::for_each(device_groups, [](VkPhysicalDeviceGroupProperties &props) { props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES, props.pNext = nullptr; });

    VK_CHECK(vkEnumeratePhysicalDeviceGroups(m_instance, &device_group_count, device_groups.data()));

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
    std::cout << "Chose physical device: " << device_properties.deviceName << "\n";
    return best_device;
}

std::vector<VkDeviceQueueCreateInfo> VulkanDevice::ChooseQueues() {
    std::vector<VkDeviceQueueCreateInfo> result {};

    static constexpr float queue_priority = 1.0f;
    constexpr uint32_t invalid_family = std::numeric_limits<uint32_t>::max();

    uint32_t graphics_compute_family = invalid_family;
    uint32_t present_family = invalid_family;
    uint32_t transfer_family = invalid_family;
    uint32_t dedicated_compute_family = invalid_family;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
    Assert(queue_family_count > 0, "There must be at least one queue family for this device!", __FILE__, __LINE__);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        const VkQueueFlags flags = queue_families[i].queueFlags;

        const bool graphics = flags & VK_QUEUE_GRAPHICS_BIT;
        const bool compute = flags & VK_QUEUE_COMPUTE_BIT;
        const bool transfer = flags & VK_QUEUE_TRANSFER_BIT;

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, m_surface, &present_support);

        if (graphics && compute && present_support) {
            graphics_compute_family = i;
            present_family = i;
        }

        if (graphics_compute_family == invalid_family && graphics && compute) {
            graphics_compute_family = i;
        }

        if (present_family == invalid_family && present_support) {
            present_family = i;
        }

        // Prefer dedicated transfer queue
        if (transfer_family == invalid_family && transfer && !graphics && !compute) {
            transfer_family = i;
        }

        if (dedicated_compute_family == invalid_family && compute && !graphics) {
            dedicated_compute_family = i;
        }
    }

    Assert(graphics_compute_family != invalid_family, "Must have a queue family that supports graphics and compute!");
    Assert(present_family != invalid_family, "Must have a queue family that supports present!");
    Assert(transfer_family != invalid_family, "Must have a dedicated transfer queue family!");
    Assert(dedicated_compute_family != invalid_family, "Must have a dedicated compute queue family!");

    Assert(graphics_compute_family == present_family, "Currently we only support present queues with graphics support!");

    m_graphics_queue.queue_family = graphics_compute_family;
    m_compute_queue.queue_family = graphics_compute_family;
    m_present_queue.queue_family = present_family;
    m_transfer_queue.queue_family = transfer_family;
    m_dedicated_compute_queue.queue_family = dedicated_compute_family;

    auto add_queue_family = [&](uint32_t family) {
        for (const auto &existing : result) {
            if (existing.queueFamilyIndex == family) {
                return;
            }
        }

        result.emplace_back(VkDeviceQueueCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        });
    };

    add_queue_family(m_graphics_queue.queue_family);
    add_queue_family(m_compute_queue.queue_family);
    add_queue_family(m_present_queue.queue_family);
    add_queue_family(m_transfer_queue.queue_family);
    add_queue_family(m_dedicated_compute_queue.queue_family);

    return result;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDevice::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data) {
    std::string prefix;
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        prefix = "VERBOSE";
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        prefix = "INFO";
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        prefix = "WARNING";
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        prefix = "ERROR";

    std::print(stderr, "Validation Message [{}]: {}\n", prefix.c_str(), callback_data->pMessage);

    return VK_FALSE;
}

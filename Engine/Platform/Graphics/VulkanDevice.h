#pragma once

#include <mutex>
#include <string>

#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include "Common.h"
#include "Platform/Window/Window.h"
#include "VulkanObjects.h"

struct DeviceConfig {
    bool enable_validation = static_cast<bool>(ENGINE_ENABLE_VALIDATION_LAYERS);
    bool enable_gpu_assisted = static_cast<bool>(ENGINE_ENABLE_GPU_ASSISTED);
    uint32_t vulkan_version = VK_MAKE_API_VERSION(0, 1, 4, 0);
};

enum class QueueType : uint8_t {
    Graphics,
    Present,
    Compute,
    Transfer,
    DedicatedCompute,
};

struct QueueSubmitInfo {
    struct SemaphoreSubmit {
        const VulkanSemaphore *semaphore = nullptr;
        uint64_t value = 0;
        VkPipelineStageFlags2 stage = 0;
    };
    
    std::vector<SemaphoreSubmit> wait_semaphores;
    std::vector<SemaphoreSubmit> signal_semaphores;
    std::vector<const CommandBuffer *> command_buffers;
};

struct SemaphoreSubmit {
    const VulkanSemaphore *semaphore = nullptr;
    uint64_t value = 0;
    VkPipelineStageFlags2 stage = 0;
};

class VulkanDevice : public NonMovable, public NonCopyable {
public:
    VulkanDevice(const Window &window, DeviceConfig config = {});
    ~VulkanDevice();

    VkInstance Instance() const { return m_instance; }
    VkDevice Device() const { return m_device; }
    VkPhysicalDevice PhysicalDevice() const { return m_physical_device; }
    VkSurfaceKHR Surface() const { return m_surface; }
    VmaAllocator Allocator() const { return m_allocator; }

    VkFormat GetLinearColorFormat() const { return m_image_formats.linear_color; }
    VkFormat GetNonLinearColorFormat() const { return m_image_formats.nonlinear_color; }
    VkFormat GetDepthStencilFormat() const { return m_image_formats.depth_stencil; }
    VkFormat GetDepthOnlyFormat() const { return m_image_formats.depth; }
    VkFormat GetHDRFormat() const { return m_image_formats.hdr; }

    bool EnabledValidations() const { return m_config.enable_validation; }

    VkQueue Queue(QueueType type) const;
    uint32_t QueueFamily(QueueType type) const;

    // Immediately submits recorded commands in the specified queue. If `async` is turned on,
    // this function will not wait for submission to be complete.
    void ImmediateSubmit(QueueType type, const std::function<void(const CommandBuffer &)> &record, bool async = false) const;
    void QueueSubmit(QueueType type, const QueueSubmitInfo &submit_info, const VulkanFence &fence);
public:
    std::unique_ptr<ShaderModule> CreateShaderModule(const std::vector<uint32_t> &spriv_code) const;

    std::unique_ptr<VulkanSemaphore> CreateBinarySemaphore() const;
    std::unique_ptr<VulkanSemaphore> CreateTimelineSemaphore(uint64_t initial_value = 0) const;
    std::unique_ptr<VulkanFence> CreateFence(bool signalled = true) const;

    std::unique_ptr<VulkanCommandPool> CreateCommandPool(QueueType queue, VkCommandPoolCreateFlags flags = 0) const;
public:
    template<typename T>
    void SetDebugName(T object, std::string_view name) const {
        if (m_config.enable_validation) {
            constexpr VkObjectType object_type = ObjectType<T>();
    
            if constexpr (object_type == VK_OBJECT_TYPE_UNKNOWN)
                return;
    
            if (object == VK_NULL_HANDLE)
                return;
    
            uint64_t object_handle = 0;
            if constexpr (std::is_pointer_v<T>) {
                object_handle = reinterpret_cast<uint64_t>(object);
            } else {
                object_handle = static_cast<uint64_t>(object);
            }
    
            std::string debug_name(name);
    
            VkDebugUtilsObjectNameInfoEXT debug_name_info {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = object_type,
                .objectHandle = object_handle,
                .pObjectName = debug_name.data(),
            };
    
            VK_CHECK(vkSetDebugUtilsObjectNameEXT(m_device, &debug_name_info));
        }
    }

private:
    const uint64_t ImmediateFenceMaxTimeout = 1000; // ms

    struct DeviceQueue {
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t queue_family = std::numeric_limits<uint32_t>::max();
    };

    struct ImmediateContext {
        VkQueue queue = VK_NULL_HANDLE;
        std::unique_ptr<VulkanCommandPool> command_pool;
        std::unique_ptr<CommandBuffer> command_buffer;
        std::unique_ptr<VulkanFence> fence;
    };

    struct ImageFormats {
        VkFormat linear_color;
        VkFormat nonlinear_color;
        VkFormat depth_stencil;
        VkFormat depth;
        VkFormat hdr;
    };
private:
    const DeviceConfig m_config {};

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;

    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    DeviceQueue m_graphics_queue {};
    DeviceQueue m_present_queue {};
    DeviceQueue m_compute_queue {};
    DeviceQueue m_transfer_queue {};
    DeviceQueue m_dedicated_compute_queue {};

    ImmediateContext m_immediate_graphics;
    ImmediateContext m_immediate_compute;
    ImmediateContext m_immediate_transfer;

    VmaAllocator m_allocator = nullptr;

    ImageFormats m_image_formats {};
private:
    void CreateVulkanInstance();
    void DestroyVulkanInstance();

    void CreateVulkanSurface(const Window &window);
    void DestroyVulkanSurface();
    
    void CreateVulkanDevice();
    void DestroyVulkanDevice();

    void CreateVulkanMemoryAllocator();
    void DestroyVulkanMemoryAllocator();

    void CreateImmediateObjects();
    void DestroyImmediateObjects();

    VkPhysicalDevice ChoosePhysicalDevice();
    std::vector<VkDeviceQueueCreateInfo> ChooseQueues();
    void ChooseImageFormats();

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data);
};

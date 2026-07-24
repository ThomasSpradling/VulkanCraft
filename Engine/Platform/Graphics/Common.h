#pragma once

#include <array>
#include <optional>
#include <string>
#include <slang/slang.h>
#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

#include "Core/errors.h"

#define VK_CHECK(expr)                                                                                              \
    if ((expr) != VK_SUCCESS) {                                                                                     \
        std::string str = "Call '" + std::string(#expr) + "' returned " + std::string(string_VkResult(expr)) + "."; \
        throw std::runtime_error(str);                                                                              \
    }

#if defined(ENGINE_ENABLE_PROFILING) && defined(ENGINE_ENABLE_GPU_PROFILING)
    #define TRACY_VK_USE_SYMBOL_TABLE
    #include <tracy/TracyVulkan.hpp>

    #define ENGINE_PROFILER_GPU_ZONE(device, command_buffer, name, color)         \
        TracyVkZoneC(device.TracyContext(), command_buffer.Handle(), name, color)

    #define ENGINE_PROFILER_GPU_COLLECT(device, command_buffer)     \
        TracyVkCollect(device.TracyContext(), command_buffer.Handle())

#else
    #define ENGINE_PROFILER_GPU_ZONE(device, command_buffer, name)
    #define ENGINE_PROFILER_GPU_COLLECT(device, command_buffer)
#endif // ENGINE_ENABLE_PROFILING

// ======================== //
// ---- Vulkan Objects ---- //
// ======================== //

template<typename T>
constexpr VkObjectType ObjectType() {
    return VK_OBJECT_TYPE_UNKNOWN;
}

#define VULKAN_OBJECT_TYPE(vk_type, object_type)          \
    template<>                                            \
    constexpr VkObjectType ObjectType<vk_type>() {        \
        return object_type;                               \
    }

    VULKAN_OBJECT_TYPE(VkInstance,               VK_OBJECT_TYPE_INSTANCE)
    VULKAN_OBJECT_TYPE(VkPhysicalDevice,         VK_OBJECT_TYPE_PHYSICAL_DEVICE)
    VULKAN_OBJECT_TYPE(VkDevice,                 VK_OBJECT_TYPE_DEVICE)
    VULKAN_OBJECT_TYPE(VkQueue,                  VK_OBJECT_TYPE_QUEUE)
    VULKAN_OBJECT_TYPE(VkSemaphore,              VK_OBJECT_TYPE_SEMAPHORE)
    VULKAN_OBJECT_TYPE(VkCommandBuffer,          VK_OBJECT_TYPE_COMMAND_BUFFER)
    VULKAN_OBJECT_TYPE(VkFence,                  VK_OBJECT_TYPE_FENCE)
    VULKAN_OBJECT_TYPE(VkDeviceMemory,           VK_OBJECT_TYPE_DEVICE_MEMORY)
    VULKAN_OBJECT_TYPE(VkBuffer,                 VK_OBJECT_TYPE_BUFFER)
    VULKAN_OBJECT_TYPE(VkImage,                  VK_OBJECT_TYPE_IMAGE)
    VULKAN_OBJECT_TYPE(VkEvent,                  VK_OBJECT_TYPE_EVENT)
    VULKAN_OBJECT_TYPE(VkQueryPool,              VK_OBJECT_TYPE_QUERY_POOL)
    VULKAN_OBJECT_TYPE(VkBufferView,             VK_OBJECT_TYPE_BUFFER_VIEW)
    VULKAN_OBJECT_TYPE(VkImageView,              VK_OBJECT_TYPE_IMAGE_VIEW)
    VULKAN_OBJECT_TYPE(VkShaderModule,           VK_OBJECT_TYPE_SHADER_MODULE)
    VULKAN_OBJECT_TYPE(VkPipelineCache,          VK_OBJECT_TYPE_PIPELINE_CACHE)
    VULKAN_OBJECT_TYPE(VkPipelineLayout,         VK_OBJECT_TYPE_PIPELINE_LAYOUT)
    VULKAN_OBJECT_TYPE(VkRenderPass,             VK_OBJECT_TYPE_RENDER_PASS)
    VULKAN_OBJECT_TYPE(VkPipeline,               VK_OBJECT_TYPE_PIPELINE)
    VULKAN_OBJECT_TYPE(VkDescriptorSetLayout,    VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
    VULKAN_OBJECT_TYPE(VkSampler,                VK_OBJECT_TYPE_SAMPLER)
    VULKAN_OBJECT_TYPE(VkDescriptorPool,         VK_OBJECT_TYPE_DESCRIPTOR_POOL)
    VULKAN_OBJECT_TYPE(VkDescriptorSet,          VK_OBJECT_TYPE_DESCRIPTOR_SET)
    VULKAN_OBJECT_TYPE(VkFramebuffer,            VK_OBJECT_TYPE_FRAMEBUFFER)
    VULKAN_OBJECT_TYPE(VkCommandPool,            VK_OBJECT_TYPE_COMMAND_POOL)
    VULKAN_OBJECT_TYPE(VkDescriptorUpdateTemplate, VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE)
    VULKAN_OBJECT_TYPE(VkSurfaceKHR,             VK_OBJECT_TYPE_SURFACE_KHR)
    VULKAN_OBJECT_TYPE(VkSwapchainKHR,           VK_OBJECT_TYPE_SWAPCHAIN_KHR)
    VULKAN_OBJECT_TYPE(VkDebugUtilsMessengerEXT, VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT)
    VULKAN_OBJECT_TYPE(VkAccelerationStructureKHR, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR)
    VULKAN_OBJECT_TYPE(VkPipelineBinaryKHR,      VK_OBJECT_TYPE_PIPELINE_BINARY_KHR)

#undef VULKAN_OBJECT_TYPE

// ========================== //
// ---- Vulkan Utilities ---- //
// ========================== //

enum class MemoryAccessType : uint8_t {
    None    = 0,
    Read    = 1 << 0,
    Write   = 1 << 1,
    ReadWrite = Read | Write,
};

bool IsDepthOnlyFormat(VkFormat format);
bool IsDepthStencilFormat(VkFormat format);
bool IsDepthFormat(VkFormat format);
VkDeviceSize GetBytesPerPixel(VkFormat format);

/**
 * @return All aspects that make up this image format.
 */
VkImageAspectFlags GetFormatAspect(VkFormat format);

/**
 * @return Gets the access flags defining when an image of this layer would be accessed for synchronization
 * purposes. `read` and `write` can be used to fine-tune which type(s) of access we expect.
 *
 * @note Should avoid using with VK_IMAGE_LAYOUT_GENERAL as this may be accessed via many ways.
 */
VkAccessFlags2 InferAccessFlags(VkImageLayout image_layout, MemoryAccessType access = MemoryAccessType::ReadWrite);

/**
 * @return Gets the pipeline stages that an image of this layout is most likely to be processed during.
 * 
 * @note Should avoid using with VK_IMAGE_LAYOUT_GENERAL as this could have been used along any pipeline.
 */
VkPipelineStageFlags2 InferPipelineStageFlags(VkImageLayout image_layout);

VkSampleCountFlagBits GetSampleCount(uint32_t sample_count);

// ======================= //
// ---- Shader Stages ---- //
// ======================= //

enum class ShaderStage : uint8_t {
    Vertex = 0,
    Fragment,
    Geometry,
    TessellationControl,
    TessellationEvaluation,
    Compute,

    Mesh,
    Task,

    RayGeneration,
    Intersection,
    AnyHit,
    ClosestHit,
    Miss,
    Callable,

    First = Vertex,
    Last = Callable,
};

inline ShaderStage operator++(ShaderStage a) {
    return static_cast<ShaderStage>(static_cast<uint8_t>(a) + 1);
}

inline constexpr VkShaderStageFlagBits GetVulkanShaderStage(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Geometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::TessellationControl:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::TessellationEvaluation:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderStage::Compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::Mesh:
            return VK_SHADER_STAGE_MESH_BIT_EXT;
        case ShaderStage::Task:
            return VK_SHADER_STAGE_TASK_BIT_EXT;
        case ShaderStage::RayGeneration:
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case ShaderStage::Intersection:
            return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case ShaderStage::AnyHit:
            return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case ShaderStage::ClosestHit:
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case ShaderStage::Miss:
            return VK_SHADER_STAGE_MISS_BIT_KHR;
        case ShaderStage::Callable:
            return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    }
    return VkShaderStageFlagBits{};
}

constexpr std::optional<ShaderStage> GetShaderStageFromSlang(SlangStage stage) {
    switch (stage) {
        case SLANG_STAGE_VERTEX:
            return ShaderStage::Vertex;
        case SLANG_STAGE_FRAGMENT:
            return ShaderStage::Fragment;
        case SLANG_STAGE_GEOMETRY:
            return ShaderStage::Geometry;
        case SLANG_STAGE_HULL:
            return ShaderStage::TessellationControl;
        case SLANG_STAGE_DOMAIN:
            return ShaderStage::TessellationEvaluation;
        case SLANG_STAGE_COMPUTE:
            return ShaderStage::Compute;
        case SLANG_STAGE_MESH:
            return ShaderStage::Mesh;
        case SLANG_STAGE_AMPLIFICATION:
            return ShaderStage::Task;
        case SLANG_STAGE_RAY_GENERATION:
            return ShaderStage::RayGeneration;
        case SLANG_STAGE_INTERSECTION:
            return ShaderStage::Intersection;
        case SLANG_STAGE_ANY_HIT:
            return ShaderStage::AnyHit;
        case SLANG_STAGE_CLOSEST_HIT:
            return ShaderStage::ClosestHit;
        case SLANG_STAGE_MISS:
            return ShaderStage::Miss;
        case SLANG_STAGE_CALLABLE:
            return ShaderStage::Callable;
        case SLANG_STAGE_NONE:
        case SLANG_STAGE_DISPATCH:
        default:
            return std::nullopt;
    }
}


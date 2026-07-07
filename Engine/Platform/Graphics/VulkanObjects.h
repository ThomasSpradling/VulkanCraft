#pragma once

#include "CommandBuffer.h"
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>
#include <volk.h>

class VulkanDevice;
enum class QueueType : uint8_t;

class ShaderModule {
public:
    ShaderModule(const VulkanDevice &device, const std::vector<uint32_t> &spriv_code);
    ~ShaderModule();

    VkShaderModule Handle() const { return m_shader_module; }
    void SetDebugName(std::string_view name) const;
private:
    const VulkanDevice &m_device;
    VkShaderModule m_shader_module = VK_NULL_HANDLE;
};

enum class SemaphoreType : uint8_t {
    Binary,
    Timeline
};

class VulkanSemaphore {
public:
    VulkanSemaphore(const VulkanDevice &device, SemaphoreType type = SemaphoreType::Binary, uint64_t initial_value = 0);
    ~VulkanSemaphore();

    VkSemaphore Handle() const { return m_semaphore; }
    void SetDebugName(std::string_view name) const;
private:
    const VulkanDevice &m_device;
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
};

class VulkanFence{
public:
    VulkanFence(const VulkanDevice &device, bool signalled);
    ~VulkanFence();

    void Wait() const;
    void Reset() const;

    VkFence Handle() const { return m_fence; }
    void SetDebugName(std::string_view name) const;
private:
    const VulkanDevice &m_device;
    VkFence m_fence = VK_NULL_HANDLE;
};

class VulkanCommandPool {
    friend CommandBuffer;
public:
    VulkanCommandPool(const VulkanDevice &device, QueueType queue, VkCommandPoolCreateFlags flags = 0);
    ~VulkanCommandPool();

    std::unique_ptr<CommandBuffer> AllocateCommandBuffer() const;
    void Reset(VkCommandPoolResetFlags flags = 0) const;

    VkCommandPool Handle() const { return m_command_pool; }
    void SetDebugName(std::string_view name) const;
private:
    const VulkanDevice &m_device;
    VkCommandPool m_command_pool = VK_NULL_HANDLE;
};

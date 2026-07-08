#include "VulkanObjects.h"
#include "VulkanDevice.h"

// Shader Module //

ShaderModule::ShaderModule(const VulkanDevice &device, const std::vector<uint32_t> &spriv_code)
    : m_device(device)
{
    VkShaderModuleCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spriv_code.size() * sizeof(uint32_t),
        .pCode = spriv_code.data(),
    };
    VK_CHECK(vkCreateShaderModule(m_device.Device(), &create_info, nullptr, &m_shader_module));
}

void ShaderModule::SetDebugName(std::string_view name) const {
    m_device.SetDebugName(m_shader_module, name);
}

ShaderModule::~ShaderModule() {
    Assert(m_shader_module, "Cannot destroy null shader module!");
    vkDestroyShaderModule(m_device.Device(), m_shader_module, nullptr);
}

// Semaphore //

VulkanSemaphore::VulkanSemaphore(const VulkanDevice &device, SemaphoreType type, uint64_t initial_value)
    : m_device(device)
{
    VkSemaphoreType semaphore_type;
    switch (type) {
        case SemaphoreType::Timeline:
            semaphore_type = VK_SEMAPHORE_TYPE_TIMELINE;
            break;
        case SemaphoreType::Binary:
        default:
            semaphore_type = VK_SEMAPHORE_TYPE_BINARY;
            initial_value = 0;
            break;
    }

    VkSemaphoreTypeCreateInfo type_create_info {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = semaphore_type,
        .initialValue = initial_value,
    };

    VkSemaphoreCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &type_create_info,
    };
    VK_CHECK(vkCreateSemaphore(m_device.Device(), &create_info, nullptr, &m_semaphore));
}

void VulkanSemaphore::SetDebugName(std::string_view name) const {
    m_device.SetDebugName(m_semaphore, name);
}

VulkanSemaphore::~VulkanSemaphore() {
    Assert(m_semaphore, "Cannot destroy null semaphore!");
    vkDestroySemaphore(m_device.Device(), m_semaphore, nullptr);
}

// Fence //

VulkanFence::VulkanFence(const VulkanDevice &device, bool signalled)
    : m_device(device)
{
    VkFenceCreateFlags flags = 0;
    if (signalled)
        flags |= VK_FENCE_CREATE_SIGNALED_BIT;

    VkFenceCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = flags,
    };
    VK_CHECK(vkCreateFence(m_device.Device(), &create_info, nullptr, &m_fence));
}

void VulkanFence::Wait() const {
    VK_CHECK(vkWaitForFences(m_device.Device(), 1, &m_fence, VK_TRUE, UINT64_MAX))
}

void VulkanFence::Reset() const {
    VK_CHECK(vkResetFences(m_device.Device(), 1, &m_fence));
}

void VulkanFence::SetDebugName(std::string_view name) const {
    m_device.SetDebugName(m_fence, name);
}

VulkanFence::~VulkanFence() {
    Assert(m_fence, "Cannot destroy null fence!");
    vkDestroyFence(m_device.Device(), m_fence, nullptr);
}

// Command Pool //

VulkanCommandPool::VulkanCommandPool(const VulkanDevice &device, QueueType queue, VkCommandPoolCreateFlags flags)
    : m_device(device)
    , m_queue(queue)
{
    VkCommandPoolCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = device.QueueFamily(queue),
    };
    VK_CHECK(vkCreateCommandPool(m_device.Device(), &create_info, nullptr, &m_command_pool));
}

VulkanCommandPool::~VulkanCommandPool() {
    vkDestroyCommandPool(m_device.Device(), m_command_pool, nullptr);
}

std::unique_ptr<CommandBuffer> VulkanCommandPool::AllocateCommandBuffer() const {
    return std::make_unique<CommandBuffer>(*this);
}

void VulkanCommandPool::Reset(VkCommandPoolResetFlags flags) const {
    vkResetCommandPool(m_device.Device(), m_command_pool, flags);
}

void VulkanCommandPool::SetDebugName(std::string_view name) const {
    m_device.SetDebugName(m_command_pool, name);
}

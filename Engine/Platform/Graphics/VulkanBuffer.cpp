#include "VulkanBuffer.h"
#include "Common.h"
#include "VulkanDevice.h"
#include <cstring>
#include <format>

// =============================== //
// ---- Vulkan Buffer Builder ---- //
// =============================== //

VulkanBufferBuilder &VulkanBufferBuilder::Size(VkDeviceSize size) {
    m_size = size;
    return *this;
}

VulkanBufferBuilder &VulkanBufferBuilder::AddUsage(VkBufferUsageFlags usage) {
    m_usage |= usage;
    return *this;
}

VulkanBufferBuilder &VulkanBufferBuilder::AddMemoryFlags(VmaAllocationCreateFlags flag) {
    m_memory_flags |= flag;
    return *this;
}

VulkanBufferBuilder &VulkanBufferBuilder::DedicateMemory() {
    m_memory_flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    return *this;
}

VulkanBufferBuilder &VulkanBufferBuilder::SharedQueueFamilies(std::span<uint32_t> queues) {
    m_queue_families = queues;
    return *this;
}

std::unique_ptr<VulkanBuffer> VulkanBufferBuilder::Build(const VulkanDevice &device) {
    VkSharingMode sharing_mode = m_queue_families.size() >= 2 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    
    VkBufferCreateInfo buffer_create_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = m_size,
        .usage = m_usage,
        .sharingMode = sharing_mode,
    };

    if (m_queue_families.size() >= 2) {
        buffer_create_info.pQueueFamilyIndices = m_queue_families.data();
        buffer_create_info.queueFamilyIndexCount = static_cast<uint32_t>(m_queue_families.size());
    }

    VmaAllocationCreateInfo allocation_create_info {
        .flags = m_memory_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
    VK_CHECK(vmaCreateBuffer(device.Allocator(), &buffer_create_info, &allocation_create_info, &buffer, &allocation, &allocation_info));
    
    auto vk_buffer = std::make_unique<VulkanBuffer>(device);
    vk_buffer->m_size = m_size;
    vk_buffer->m_usage = m_usage;
    vk_buffer->m_buffer = buffer;
    vk_buffer->m_allocation = allocation;
    vk_buffer->m_allocation_info = allocation_info;
    vk_buffer->m_memory_flags = m_memory_flags;
    vk_buffer->m_initialized = true;
    vk_buffer->m_queue_families = m_queue_families;

    return vk_buffer;
}

// ======================= //
// ---- Vulkan Buffer ---- //
// ======================= //

VulkanBuffer::VulkanBuffer(const VulkanDevice &device)
    : m_device(device)
{}

VulkanBuffer::~VulkanBuffer() {
    vmaDestroyBuffer(m_device.Allocator(), m_buffer, m_allocation);
}

VkDeviceAddress VulkanBuffer::DeviceAddress() const {
    VkBufferDeviceAddressInfo info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = m_buffer,
    };
    return vkGetBufferDeviceAddress(m_device.Device(), &info);
}

void *VulkanBuffer::Mapped() {
    Assert(m_memory_flags & VMA_ALLOCATION_CREATE_MAPPED_BIT,
        "Cannot map memory of VulkanBuffer without VMA_ALLOCATION_CREATE_MAPPED_BIT set!");

    return m_allocation_info.pMappedData;
}

void VulkanBuffer::Resize(VkDeviceSize size) {
    Assert(m_initialized, "Cannot resize to un-initialized VulkanBuffer");
    
    vmaDestroyBuffer(m_device.Allocator(), m_buffer, m_allocation);
    auto buffer = VulkanBuffer::BufferBuilder()
        .Size(size)
        .AddMemoryFlags(m_memory_flags)
        .SharedQueueFamilies(m_queue_families)
        .AddUsage(m_usage)
        .Build(m_device);

    m_size = buffer->m_size;
    m_usage = buffer->m_usage;
    m_buffer = buffer->m_buffer;
    m_allocation = buffer->m_allocation;
    m_allocation_info = buffer->m_allocation_info;
    m_memory_flags = buffer->m_memory_flags;
    m_initialized = true;
    m_queue_families = buffer->m_queue_families;
}

void VulkanBuffer::Upload(const void *data, VkDeviceSize bytes, VkDeviceSize offset) {
    bool has_required_flags = m_memory_flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        || m_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT; 
    
    Assert(m_initialized, "Cannot upload to un-initialized VulkanBuffer");
    Assert(has_required_flags, "Cannot upload to buffer that is neither mapped nor accepts transfers! "
                            "You must either include the usage VK_BUFFER_USAGE_TRANSFER_DST_BIT or "
                            "include the memory flag VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT.");

    if (m_memory_flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) {
        // Directly map memory

        if (m_memory_flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
            std::memcpy(m_allocation_info.pMappedData, data, bytes);
            return;
        }

        void *mapped;
        VK_CHECK(vmaMapMemory(m_device.Allocator(), m_allocation, &mapped));
        std::memcpy(mapped, data, bytes);
        vmaUnmapMemory(m_device.Allocator(), m_allocation);
        return;
    }

    // Must have TRANSFER_DST to be used with staging buffer
    auto staging_buffer = VulkanBuffer::BufferBuilder()
        .Size(bytes)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Build(m_device);
    
    std::memcpy(staging_buffer->Mapped(), data, bytes);

    m_device.ImmediateSubmit(QueueType::Graphics, [&](VkCommandBuffer cmd) {
        VkBufferCopy copy_region {
            .srcOffset = 0,
            .dstOffset = offset,
        };
        vkCmdCopyBuffer(cmd, staging_buffer->Buffer(), m_buffer, 1, &copy_region);
    });
}

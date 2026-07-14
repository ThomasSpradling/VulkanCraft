#include "VulkanBuffer.h"
#include "Common.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Platform/Graphics/Common.h"
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

std::unique_ptr<VulkanBuffer> VulkanBufferBuilder::Build() {
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
    VK_CHECK(vmaCreateBuffer(m_device.Allocator(), &buffer_create_info, &allocation_create_info, &buffer, &allocation, &allocation_info));

    auto vk_buffer = std::make_unique<VulkanBuffer>(m_device);
    vk_buffer->m_size = m_size;
    vk_buffer->m_usage = m_usage;
    vk_buffer->m_buffer = buffer;
    vk_buffer->m_allocation = allocation;
    vk_buffer->m_allocation_info = allocation_info;
    vk_buffer->m_memory_flags = m_memory_flags;
    vk_buffer->m_initialized = true;
    vk_buffer->m_queue_families = m_queue_families;

    if (m_data != nullptr) {
        vk_buffer->Upload(m_data, m_size);
    }

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

void VulkanBuffer::SetDebugName(std::string_view name) const {
    m_device.SetDebugName(m_buffer, name);
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
    auto buffer = VulkanBuffer::BufferBuilder(m_device)
        .Size(size)
        .AddMemoryFlags(m_memory_flags)
        .SharedQueueFamilies(m_queue_families)
        .AddUsage(m_usage)
        .Build();

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
    Assert(m_initialized, "Cannot upload to un-initialized VulkanBuffer.");
    Assert(data != nullptr, "Cannot upload from null data.");
    Assert(offset <= m_size || offset < 0, "Upload offset is outside the buffer.");
    Assert(bytes <= m_size - offset, "Upload exceeds buffer size.");

    const bool has_host_access = (m_memory_flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) != 0
        || (m_memory_flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT) != 0;
    const bool accepts_transfer = (m_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0;

    Assert(has_host_access || accepts_transfer, "Cannot upload to buffer that is neither mapped nor accepts transfers! "
                            "You must either include the usage VK_BUFFER_USAGE_TRANSFER_DST_BIT or "
                            "include the memory flag VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT.");

    if (has_host_access) {
        VK_CHECK(vmaCopyMemoryToAllocation(m_device.Allocator(), data, m_allocation, offset, bytes));
        return;
    }

    auto staging_buffer = VulkanBuffer::BufferBuilder(m_device)
        .Size(bytes)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Build();
    staging_buffer->Upload(data, bytes, 0);
    
    m_device.ImmediateSubmit(QueueType::Graphics, [&](const CommandBuffer &cmd) {
        VkBufferCopy copy_region {
            .srcOffset = 0,
            .dstOffset = offset,
            .size = bytes,
        };
        vkCmdCopyBuffer(cmd.Handle(), staging_buffer->Buffer(), m_buffer, 1, &copy_region);
    });
}

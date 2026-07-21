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

    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(m_device.Device(), buffer, &requirements);

    bool is_coherent = false;
    if (requirements.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        is_coherent = true;
    }

    auto vk_buffer = std::make_unique<VulkanBuffer>(m_device);
    vk_buffer->m_size = m_size;
    vk_buffer->m_usage = m_usage;
    vk_buffer->m_buffer = buffer;
    vk_buffer->m_allocation = allocation;
    vk_buffer->m_allocation_info = allocation_info;
    vk_buffer->m_memory_flags = m_memory_flags;
    vk_buffer->m_initialized = true;
    vk_buffer->m_queue_families = m_queue_families;
    vk_buffer->m_is_coherent = is_coherent;

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

void VulkanBuffer::Destroy() {
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

    Assert(m_is_coherent, "Currently does not support user mapping of non-coherent memory!");

    return m_allocation_info.pMappedData;
}

void VulkanBuffer::FlushMappedMemory(VkDeviceSize offset, VkDeviceSize size) {
    if (!IsMapped())
        return;
    
    VK_CHECK(vmaFlushAllocation(m_device.Allocator(), m_allocation, offset, size));
}

void VulkanBuffer::InvalidateMappedMemory(VkDeviceSize offset, VkDeviceSize size) {
    if (!IsMapped())
        return;

    VK_CHECK(vmaInvalidateAllocation(m_device.Allocator(), m_allocation, offset, size));
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

    const bool mapped = (m_memory_flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0;
    const bool has_host_access = (m_memory_flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) != 0
        || (m_memory_flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT) != 0;
    const bool accepts_transfer = (m_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0;

    Assert((has_host_access & mapped) || accepts_transfer, "In order to upload data, it must either be mapped or allow for transfers!");

    if (mapped && has_host_access) {
        uint8_t *dst = static_cast<uint8_t *>(m_allocation_info.pMappedData) + offset;
        if (data) {
            std::memcpy(dst, data, bytes);
        } else {
            std::memset(dst, 0, bytes);
        }

        if (!m_is_coherent)
            FlushMappedMemory(offset, bytes);

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

std::vector<std::byte> VulkanBuffer::ReadData(VkDeviceSize bytes, VkDeviceSize offset) {
    Assert(m_initialized, "Cannot upload to un-initialized VulkanBuffer.");
    Assert(offset <= m_size || offset < 0, "Download offset is outside the buffer.");
    Assert(bytes <= m_size - offset, "Download exceeds buffer size.");
    
    const bool mapped = (m_memory_flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0;
    const bool has_host_access = (m_memory_flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT) != 0;
    const bool accepts_transfer = (m_usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != 0;

    Assert((mapped & has_host_access) | accepts_transfer, "In order to download data, it must either be mapped or allow for transfers!");

    std::vector<std::byte> result(bytes);
    if (mapped && has_host_access) {
        uint8_t *src = static_cast<uint8_t *>(m_allocation_info.pMappedData) + offset;

        if (!m_is_coherent)
            InvalidateMappedMemory(offset, bytes);

        std::memcpy(result.data(), src, bytes);
        return result;
    }

    auto staging_buffer = VulkanBuffer::BufferBuilder(m_device)
        .Size(bytes)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Build();

    m_device.ImmediateSubmit(QueueType::Graphics, [&](const CommandBuffer &cmd) {
        VkBufferCopy copy_region {
            .srcOffset = offset,
            .dstOffset = 0,
            .size = bytes,
        };
        vkCmdCopyBuffer(cmd.Handle(), m_buffer, staging_buffer->Buffer(), 1, &copy_region);
    });

    return staging_buffer->ReadData(bytes, 0);
}

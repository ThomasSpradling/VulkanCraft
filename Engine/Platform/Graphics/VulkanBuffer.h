#pragma once

#include "Common.h"
#include "Core/NonCopyable.h"
#include "VulkanDevice.h"
#include <initializer_list>
#include <span>

class VulkanBuffer;
class VulkanBufferBuilder {
    friend VulkanBuffer;
public:
    VulkanBufferBuilder() = default;
    VulkanBufferBuilder &Size(VkDeviceSize size);
    VulkanBufferBuilder &AddUsage(VkBufferUsageFlags usage);
    VulkanBufferBuilder &AddMemoryFlags(VmaAllocationCreateFlags flag);
    VulkanBufferBuilder &DebugName(const std::string &name);
    
    // For larger data to ensure that this memory has its own dedicated
    // memory block.
    VulkanBufferBuilder &DedicateMemory();
    
    VulkanBufferBuilder &SharedQueueFamilies(std::span<uint32_t> queues);
    std::unique_ptr<VulkanBuffer> Build(const VulkanDevice &device);
private:
    std::string m_debug_name = "";

    VkDeviceSize m_size = 0ull;
    VkBufferUsageFlags m_usage = 0;
    VmaAllocationCreateFlags m_memory_flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    std::span<uint32_t> m_queue_families {}; // Queue families that may access this memory
};

class VulkanBuffer : public NonCopyable {
    friend VulkanBufferBuilder;
public:
    VulkanBuffer(const VulkanDevice &device);
    ~VulkanBuffer();

    static inline VulkanBufferBuilder BufferBuilder() { return VulkanBufferBuilder(); }

    void SetDebugName(std::string_view name) const;

    VkBuffer Buffer() const { return m_buffer; }
    VmaAllocation Allocation() const { return m_allocation; }
    VkDeviceSize Size() const { return m_size; }

    VkDeviceAddress DeviceAddress() const;

    void *Mapped();

    // Resizes buffer.
    // Warning: Does not check or handle any synchronization to ensure that
    // this buffer is not in use. This MUST be handled by the caller! Therefore,
    // for now, one should NOT call this often!
    void Resize(VkDeviceSize size);

    void Upload(const void *data, VkDeviceSize bytes, VkDeviceSize offset = 0ull);

    template <typename T>
    void Upload(const std::vector<T> &data, VkDeviceSize buffer_offset = 0) {
        Upload(data.data(), data.size() * sizeof(T), buffer_offset);
    }
    
    template <typename T>
    T *Mapped() {
        return static_cast<T *>(Mapped());
    }
private:
    const VulkanDevice &m_device;

    bool m_initialized = false;

    VkDeviceSize m_size = 0ull;
    VkBufferUsageFlags m_usage = 0;
    VmaAllocationCreateFlags m_memory_flags = 0;
    
    std::span<uint32_t> m_queue_families {};
    
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = nullptr;
    VmaAllocationInfo m_allocation_info {};
};

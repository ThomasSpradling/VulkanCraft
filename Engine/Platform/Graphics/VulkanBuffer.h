#pragma once

#include "Common.h"
#include "Core/NonCopyable.h"
#include "VulkanDevice.h"
#include <initializer_list>
#include <span>
#include <type_traits>

class VulkanBuffer;
class VulkanBufferBuilder {
    friend VulkanBuffer;
public:
    VulkanBufferBuilder(const VulkanDevice &device) : m_device(device) {};
    VulkanBufferBuilder &Size(VkDeviceSize size);
    
    VulkanBufferBuilder &AddUsage(VkBufferUsageFlags usage);
    VulkanBufferBuilder &AddMemoryFlags(VmaAllocationCreateFlags flag);
    
    // For larger data to ensure that this memory has its own dedicated
    // memory block.
    VulkanBufferBuilder &DedicateMemory();
    
    VulkanBufferBuilder &SharedQueueFamilies(std::span<uint32_t> queues);
    std::unique_ptr<VulkanBuffer> Build();
private:
    const VulkanDevice &m_device;

    const void *m_data = nullptr;
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

    void Destroy();

    static inline VulkanBufferBuilder BufferBuilder(const VulkanDevice &device) { return VulkanBufferBuilder(device); }
    void SetDebugName(std::string_view name) const;

    VkBuffer Buffer() const { return m_buffer; }
    VmaAllocation Allocation() const { return m_allocation; }
    VkDeviceSize Size() const { return m_size; }

    VkDeviceAddress DeviceAddress() const;

    bool IsMapped() const { return m_allocation_info.pMappedData != nullptr; }

    void *Mapped();
    void FlushMappedMemory(VkDeviceSize offset, VkDeviceSize size);
    void InvalidateMappedMemory(VkDeviceSize offset, VkDeviceSize size);

    // Resizes buffer.
    // Warning: Does not check or handle any synchronization to ensure that
    // this buffer is not in use. This MUST be handled by the caller! Therefore,
    // for now, one should NOT call this often!
    void Resize(VkDeviceSize size);
    void Upload(const void *data, VkDeviceSize bytes, VkDeviceSize offset = 0ull);
    std::vector<std::byte> ReadData(VkDeviceSize bytes, VkDeviceSize offset = 0ull);
    
    template<typename T>
    std::vector<T> ReadDataArray(size_t count, VkDeviceSize offset = 0ull) {
        static_assert(std::is_trivially_copyable_v<T>);

        const auto byte_count = static_cast<VkDeviceSize>(sizeof(T) * count);
        std::vector<std::byte> bytes = ReadData(byte_count, offset);
        std::vector<T> result(count);

        if (!bytes.empty()) {
            std::memcpy(result.data(), bytes.data(), static_cast<std::size_t>(byte_count));
        }

        return result;
    }

    template<typename T>
    T ReadData(VkDeviceSize offset = 0ull) {
        static_assert(std::is_trivially_copyable_v<T>);

        std::vector<std::byte> bytes = ReadData(sizeof(T), offset);

        T result {};
        std::memcpy(&result, bytes.data(), sizeof(T));
        return result;
    }

    // Upload data to index, viewing this buffer as an array<T>
    template<typename T>
    void UploadAt(const T *data, uint32_t index) {
        Upload(data, sizeof(T), sizeof(T) * index);
    }

    template<typename T>
    void Upload(const std::vector<T> &data, VkDeviceSize buffer_offset = 0) {
        Upload(data.data(), data.size() * sizeof(T), buffer_offset);
    }
    
    template<typename T>
    T *Mapped() {
        return static_cast<T *>(Mapped());
    }
private:
    const VulkanDevice &m_device;

    bool m_initialized = false;
    bool m_is_coherent = false;

    VkDeviceSize m_size = 0ull;
    VkBufferUsageFlags m_usage = 0;
    VmaAllocationCreateFlags m_memory_flags = 0;
    
    std::span<uint32_t> m_queue_families {};
    
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = nullptr;
    VmaAllocationInfo m_allocation_info {};
};

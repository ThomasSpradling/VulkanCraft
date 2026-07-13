#pragma once

#include "Platform/Graphics/DescriptorAllocator.h"
#include "Platform/Graphics/VulkanImage.h"
#include <memory>
#include <vector>

[[maybe_unused]] constexpr uint32_t MaxTextures               = 16'384;
[[maybe_unused]] constexpr uint32_t MaxSamplers               = 256;
[[maybe_unused]] constexpr uint32_t MaxStorageImages          = 2'048;
[[maybe_unused]] constexpr uint32_t MaxAccelerationStructures = 16;

enum class DescriptorBinding : uint8_t {
    Textures               = 0,
    Samplers               = 1,
    StorageImages          = 2,
    AccelerationStructures = 3,
};

using TextureId = uint32_t;
using SamplerId = uint32_t;
using StorageImageId = uint32_t;
using AccelerationStructureId = uint32_t;

inline constexpr TextureId DefaultTextureId = 0;
inline constexpr SamplerId DefaultSamplerId = 0;
inline constexpr StorageImageId DefaultStorageImageId = 0;
inline constexpr AccelerationStructureId DefaultAccelerationStructureId = 0;

class GPUResourceManager {
public:
    GPUResourceManager(const VulkanDevice &device);
    ~GPUResourceManager();

    VkDescriptorSetLayout GlobalDescriptorLayout() const { return m_global_descriptor_layout; }
    VkDescriptorSet GlobalDescriptorSet() const { return m_global_descriptor_set; }
    
    // Note: Anything added here takes ownership away from caller
    TextureId AddTexture(std::unique_ptr<VulkanImage> &image);
    SamplerId AddSampler(VkSampler sampler);
    StorageImageId AddStorageImage(std::unique_ptr<VulkanImage> &image);
    AccelerationStructureId AddAccelerationStructure(VkAccelerationStructureKHR acceleration_structure);

    VulkanImage &GetTexture(TextureId id);
    VkSampler GetSampler(SamplerId id);
    VulkanImage &GetStorageImage(StorageImageId id);
    VkAccelerationStructureKHR GetAccelerationStructure(AccelerationStructureId id);
private:
    template <size_t Capacity>
    struct FreeList {
        std::vector<uint32_t> data;

        FreeList() {
            static_assert(Capacity > 1);
            data.reserve(Capacity - 1);

            for (uint32_t i = Capacity - 1; i > 0; --i)
                data.push_back(i);
        }

        uint32_t Acquire() {
            Assert(!data.empty(), "Descriptor table is full!");

            uint32_t id = data.back();
            data.pop_back();
            return id;
        }
      
        void Release(uint32_t id) {
            Assert(id > 0 && id < Capacity, "Invalid id");

            data.push_back(id);
            return;
        }
    };
private:
    const VulkanDevice &m_device;

    std::unique_ptr<DescriptorAllocator> m_descriptor_allocator;
    VkDescriptorSetLayout m_global_descriptor_layout = VK_NULL_HANDLE;
    VkDescriptorSet m_global_descriptor_set = VK_NULL_HANDLE;

    FreeList<MaxTextures> m_texture_ids;
    FreeList<MaxSamplers> m_sampler_ids;
    FreeList<MaxStorageImages> m_storage_image_ids;
    FreeList<MaxAccelerationStructures> m_acceleration_structure_ids;

    std::array<std::unique_ptr<VulkanImage>, MaxTextures> m_textures {};
    std::array<VkSampler, MaxSamplers> m_samplers {};
    std::array<std::unique_ptr<VulkanImage>, MaxStorageImages> m_storage_images {};
    std::array<VkAccelerationStructureKHR, MaxAccelerationStructures> m_acceleration_structures {};
private:
    void WriteTexture(TextureId id, const VulkanImage &image);
    void WriteSampler(SamplerId id, VkSampler sampler);
    void WriteStorageImage(StorageImageId id, const VulkanImage &image);
    void WriteAccelerationStructure(AccelerationStructureId id, VkAccelerationStructureKHR acceleration_structure);
};

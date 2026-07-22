#pragma once

#include "Platform/Graphics/DescriptorAllocator.h"
#include "Platform/Graphics/VulkanImage.h"
#include <memory>
#include <vector>
#include "AssetManager/GPUStructs.h"
#include "Core/IndexFreeList.h"

[[maybe_unused]] constexpr uint32_t MaxTextures               = 16'384;
[[maybe_unused]] constexpr uint32_t MaxCubeTextures           = 256;
[[maybe_unused]] constexpr uint32_t MaxSamplers               = 256;
[[maybe_unused]] constexpr uint32_t MaxStorageImages          = 2'048;
[[maybe_unused]] constexpr uint32_t MaxAccelerationStructures = 16;

enum class DescriptorBinding : uint8_t {
    Textures_2D            = 0,
    Textures_Cube          = 1,
    Samplers               = 2,
    StorageImages          = 3,
    AccelerationStructures = 4,
};

inline constexpr TextureId DefaultTextureId = 0;
inline constexpr TextureId DefaultCubeTextureId = 0;
inline constexpr SamplerId DefaultSamplerId = 0;
inline constexpr StorageImageId DefaultStorageImageId = 0;
inline constexpr AccelerationStructureId DefaultAccelerationStructureId = 0;

class BindlessDescriptorTable {
public:
    BindlessDescriptorTable(const VulkanDevice &device);
    ~BindlessDescriptorTable();

    VkDescriptorSetLayout GlobalDescriptorLayout() const { return m_global_descriptor_layout; }
    VkDescriptorSet GlobalDescriptorSet() const { return m_global_descriptor_set; }
    
    // Note: Anything added here takes ownership away from caller
    TextureId AddTexture(std::unique_ptr<VulkanImage> &image);
    TextureId AddTextureCube(std::unique_ptr<VulkanImage> &image);
    SamplerId AddSampler(VkSampler sampler);
    StorageImageId AddStorageImage(std::unique_ptr<VulkanImage> &image);
    AccelerationStructureId AddAccelerationStructure(VkAccelerationStructureKHR acceleration_structure);

    VulkanImage &GetTexture(TextureId id);
    VulkanImage &GetTextureCube(TextureId id);
    VkSampler GetSampler(SamplerId id);
    VulkanImage &GetStorageImage(StorageImageId id);
    VkAccelerationStructureKHR GetAccelerationStructure(AccelerationStructureId id);

    void RemoveTexture(TextureId id);
    void RemoveTextureCube(TextureId id);
    void RemoveSampler(SamplerId id);

    void ReplaceTexture(TextureId id, std::unique_ptr<VulkanImage> image);
    void ReplaceSampler(SamplerId id, VkSampler sampler);

    const VulkanDevice &Device() const { return m_device; }
private:
    const VulkanDevice &m_device;

    std::unique_ptr<DescriptorAllocator> m_descriptor_allocator;
    VkDescriptorSetLayout m_global_descriptor_layout = VK_NULL_HANDLE;
    VkDescriptorSet m_global_descriptor_set = VK_NULL_HANDLE;

    IndexFreeList<MaxTextures> m_texture_ids;
    IndexFreeList<MaxTextures> m_cube_texture_ids;
    IndexFreeList<MaxSamplers> m_sampler_ids;
    IndexFreeList<MaxStorageImages> m_storage_image_ids;
    IndexFreeList<MaxAccelerationStructures> m_acceleration_structure_ids;

    std::array<std::unique_ptr<VulkanImage>, MaxTextures> m_textures {};
    std::array<std::unique_ptr<VulkanImage>, MaxCubeTextures> m_cube_textures {};
    std::array<VkSampler, MaxSamplers> m_samplers {};
    std::array<std::unique_ptr<VulkanImage>, MaxStorageImages> m_storage_images {};
    std::array<VkAccelerationStructureKHR, MaxAccelerationStructures> m_acceleration_structures {};
private:
    void WriteTexture(TextureId id, const VulkanImage &image);
    void WriteTextureCube(TextureId id, const VulkanImage &image);
    void WriteSampler(SamplerId id, VkSampler sampler);
    void WriteStorageImage(StorageImageId id, const VulkanImage &image);
    void WriteAccelerationStructure(AccelerationStructureId id, VkAccelerationStructureKHR acceleration_structure);
};

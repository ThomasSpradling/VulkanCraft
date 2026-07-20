#pragma once

#include "Buffer.h"
#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include "GPUStructs.h"
#include "Mesh.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanDevice.h"
#include "Renderer/BindlessDescriptorTable.h"
#include "Platform/Graphics/VulkanImage.h"
#include "Shape.h"
#include "Texture.h"
#include <filesystem>
#include <future>
#include <type_traits>
#include <vector>
#include "Core/ResourcePool.h"
#include "Renderer/GarbageCollector.h"

[[maybe_unused]] constexpr uint32_t MaxPbrMaterials = 1'024;
[[maybe_unused]] constexpr uint32_t MaxBasicMaterials = 1'024;

class GLTFModel;
class AssetManager : public NonCopyable, public NonMovable {
public:
    enum class MaterialType : uint8_t {
        MetallicRoughness,
        Basic,
    };

    struct MaterialRecord {
        uint32_t material_index;
        MaterialType type;
    };

    struct TextureRecord {
        TextureId texture_id;
    };

    struct SamplerRecord {
        SamplerId sampler_id;
    };
public:
    AssetManager(const VulkanDevice &device, GarbageCollector &garbage_collector, BindlessDescriptorTable &descriptor_table);
    ~AssetManager();
    
    TextureHandle LoadTexture(const std::filesystem::path &path, TextureFormat format = TextureFormat::RGBA8Srgb);
    
    GLTFHandle LoadGLTF(const std::filesystem::path &path);
    MeshHandle CreateMesh(const std::vector<MeshVertex> &vertices, const std::vector<uint32_t> &indices);
    MeshHandle CreateMesh(const Shape &shape);
    
    MaterialHandle CreateMaterial(const MetallicRoughnessMaterial &material);
    MaterialHandle CreateMaterial(const BasicMaterial &material);

    SamplerHandle CreateSampler(const TextureSamplerData &sampler);
    TextureHandle CreateTexture(const Texture &texture);

    BufferHandle CreateBuffer(const GPUBufferData &buffer);

    const VulkanBuffer &MaterialBuffer(MaterialType type = MaterialType::MetallicRoughness) const;
    
    const GLTFModel &GetGLTF(GLTFHandle mesh);
    const Mesh &GetMesh(MeshHandle mesh);
    const MaterialRecord &GetMaterial(MaterialHandle material);
    std::pair<SamplerId, VkSampler> GetSampler(SamplerHandle material);
    std::pair<TextureId, std::reference_wrapper<VulkanImage>> GetTexture(TextureHandle texture);

    VulkanBuffer &GetBuffer(BufferHandle buffer);
    const VulkanBuffer &GetBuffer(BufferHandle buffer) const;

    void DestroyGLTF(GLTFHandle handle);
    void DestroyMesh(MeshHandle handle);
    void DestroyMaterial(MaterialHandle handle);
    void DestroySampler(SamplerHandle handle);
    void DestroyTexture(TextureHandle handle);
    void DestroyBuffer(BufferHandle handle);
private:
    const VulkanDevice &m_device;

    ResourcePool<std::unique_ptr<GLTFModel>, GLTFHandleTag> m_gltf_models;
    ResourcePool<std::unique_ptr<Mesh>, MeshHandleTag> m_meshes;

    ResourcePool<TextureRecord, TextureHandleTag> m_textures;
    ResourcePool<SamplerRecord, SamplerHandleTag> m_samplers;
    ResourcePool<MaterialRecord, MaterialHandleTag> m_materials;

    ResourcePool<std::unique_ptr<VulkanBuffer>, BufferHandleTag> m_buffers;

    // GPU resources
    // std::unique_ptr<VulkanBuffer> m_pbr_material_buffer;
    BufferHandle m_pbr_material_buffer = BufferHandle::Invalid();
    uint32_t m_pbr_material_buffer_index = 0;
    
    // std::unique_ptr<VulkanBuffer> m_basic_material_buffer;
    BufferHandle m_basic_material_buffer = BufferHandle::Invalid();
    uint32_t m_basic_material_buffer_index = 0;

    BindlessDescriptorTable &m_bindless_table;
    GarbageCollector &m_garbage_collector;
private:
    TextureId GetTextureId(TextureHandle texture);
    SamplerId GetSamplerId(SamplerHandle sampler);
};
#pragma once

#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include "GPUStructs.h"
#include "Mesh.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanDevice.h"
#include "Renderer/BindlessDescriptorTable.h"
#include "Platform/Graphics/VulkanImage.h"
#include "Texture.h"
#include <filesystem>
#include <type_traits>
#include <vector>

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
    AssetManager(const VulkanDevice &device, BindlessDescriptorTable &descriptor_table);
    ~AssetManager();

    TextureHandle LoadTexture(const std::filesystem::path &path);
    
    GLTFHandle LoadGLTF(const std::filesystem::path &path);
    MeshHandle CreateMesh(const std::vector<MeshVertex> &vertices, const std::vector<uint32_t> &indices);
    
    MaterialHandle CreateMaterial(const MetallicRoughnessMaterial &material);
    MaterialHandle CreateMaterial(const BasicMaterial &material);

    SamplerHandle CreateSampler(const TextureSamplerData &sampler);
    TextureHandle CreateTexture(const TextureData &texture);

    const VulkanBuffer &MaterialBuffer(MaterialType type = MaterialType::MetallicRoughness) const;
    
    const GLTFModel &GetGLTF(GLTFHandle mesh);
    const Mesh &GetMesh(MeshHandle mesh);
    const MaterialRecord &GetMaterial(MaterialHandle material);
    std::pair<SamplerId, VkSampler> GetSampler(SamplerHandle material);
    std::pair<TextureId, std::reference_wrapper<VulkanImage>> GetTexture(TextureHandle texture);
private:
    template<typename T, typename Tag>
    struct ResourcePool {
        std::vector<T> data;
        std::vector<uint32_t> generations;
        std::vector<uint32_t> available_indices;

        ResourcePool() {
            data.resize(1'000);
            generations.resize(1'000, 0);
        }

        Handle<Tag> Add(T &record) {
            uint32_t index;

            if (!available_indices.empty()) {
                index = available_indices.back();
                available_indices.pop_back();

                data[index] = std::move(record);
            } else {
                index = static_cast<uint32_t>(data.size());
                data.push_back(std::move(record));
                generations.push_back(0);
            }

            Handle<Tag> handle;
            handle.m_index = index;
            handle.m_generation = generations[index];
            return handle;
        }
        
        T &Get(Handle<Tag> handle) {
            Assert(handle.m_index < data.size(), "Cannot get mesh thats out of range!");
            Assert(handle.m_index < generations.size(), "Cannot get mesh thats out of range!");
            Assert(handle.m_generation == generations[handle.m_index], "Mismatching generations for meshes!");

            return data[handle.m_index];
        }

        void Remove(Handle<Tag> handle) {
            if (handle.m_index >= data.size() || handle.m_index >= generations.size() || handle.m_generation != generations[handle.m_index]) {
                return;
            }

            data[handle.m_index] = T{};
            ++generations[handle.m_index];
            available_indices.push_back(handle.m_index);
        }
    };
private:
    const VulkanDevice &m_device;

    ResourcePool<std::unique_ptr<GLTFModel>, GLTFHandleTag> m_gltf_models;
    ResourcePool<std::unique_ptr<Mesh>, MeshHandleTag> m_meshes;
    ResourcePool<TextureRecord, TextureHandleTag> m_textures;
    ResourcePool<SamplerRecord, SamplerHandleTag> m_samplers;
    ResourcePool<MaterialRecord, MaterialHandleTag> m_materials;

    // GPU resources
    std::unique_ptr<VulkanBuffer> m_pbr_material_buffer;
    uint32_t m_pbr_material_buffer_index = 0;
    
    std::unique_ptr<VulkanBuffer> m_basic_material_buffer;
    uint32_t m_basic_material_buffer_index = 0;

    BindlessDescriptorTable &m_bindless_table;
private:
    TextureId GetTextureId(TextureHandle texture);
    SamplerId GetSamplerId(SamplerHandle sampler);
};
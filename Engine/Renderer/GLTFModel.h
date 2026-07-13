#pragma once

#include "GPUResourceManager.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanDevice.h"
#include <filesystem>
#include <functional>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <memory>

#include <optional>
#include <tiny_gltf.h>
#include <tuple>
#include <variant>
#include <vector>


class GLTFModel {
public:
    struct Primitive {
        uint32_t start_index = 0;
        uint32_t index_count = 0;
        uint32_t material_index = 0;
    };

    struct Mesh {
        std::vector<Primitive> primitives;
    };

    struct Node {
        Node *parent = nullptr;
        std::vector<std::shared_ptr<Node>> children {};
        std::optional<Mesh> mesh = std::nullopt;

        // Local transforms
        glm::dvec3 translation {};
        glm::dquat rotation { 1.0, 0.0, 0.0, 0.0 };
        glm::dvec3 scale { 1.0 };
        glm::dmat4 matrix { 1.0 };

        // Cache
        bool local_dirty = true;
        glm::dmat4 cached_local_transform { 1.0 };
        bool global_dirty = true;
        glm::dmat4 cached_global_transform { 1.0 };

        glm::dmat4 ComputeLocalTransform();
        glm::dmat4 ComputeGlobalTransform();
        void Update();
    };
public:
    GLTFModel(const VulkanDevice &device, GPUResourceManager &resource_manager, const std::filesystem::path &file_path);
    ~GLTFModel();

    void Update();

    void ForEachNode(const std::function<void(Node &)> &callback);
    const VulkanBuffer &VertexBuffer() const { return *m_vertex_buffer; }
    const VulkanBuffer &IndexBuffer() const { return *m_index_buffer; }
    const VulkanBuffer &MaterialBuffer() const { return *m_material_buffer; }
private:
    struct GPUMaterial {
        glm::vec4 color_factors;

        uint32_t albedo_texcoord;
        uint32_t normal_texcoord;

        TextureId albedo_texture;
        SamplerId albedo_sampler;

        TextureId normal_texture;
        SamplerId normal_sampler;
    };

    struct Material {
        glm::vec4 color_factors {};

        uint32_t albedo_texcoord = 0;
        uint32_t normal_texcoord = 0;

        uint32_t albedo_texture_index = 0;
        uint32_t normal_texture_index = 0;
    };

    struct MeshVertex {
        glm::vec3 position {};
        glm::vec3 normal {};
        glm::vec4 tangent {};
        glm::vec2 uv0 {};
        glm::vec2 uv1 {};
        glm::vec4 color {};
        glm::uvec2 joints0 {}; // 4 shorts packed
        glm::vec4 weights0 {};
    };

    struct Texture {
        TextureId srgb_texture_id = 0;
        TextureId unorm_texture_id = 0;

        SamplerId sampler_id = 0;

        // Whether this texture needs an sRGB and/or UNORM gpu representation for this GLTF file
        bool needs_srgb = false;
        bool needs_unorm = false;
    };
private:
    using ComponentType = std::variant<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float>;
private:
    const VulkanDevice &m_device;
    GPUResourceManager &m_resource_manager;

    struct VertexData {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
    };
    
    std::vector<std::shared_ptr<Node>> m_root_nodes {};

    // Flattened array of all nodes
    std::vector<std::shared_ptr<Node>> m_linear_nodes {};

    // Index zero is reserved for default texture
    std::vector<Texture> m_textures {};

    // Index zero is reserved as the default material
    std::vector<Material> m_materials {};

    std::unique_ptr<VulkanBuffer> m_vertex_buffer;
    std::unique_ptr<VulkanBuffer> m_index_buffer;
    std::unique_ptr<VulkanBuffer> m_material_buffer;
private:
    void LoadTextures(tinygltf::Model &model);
    void LoadMaterials(tinygltf::Model &model);
    void LoadNode(Node *parent, tinygltf::Node &node, tinygltf::Model &model, VertexData &data);

    // Callback runs over (index, accessor_value, accessor_value_type)
    void IterateAccessor(tinygltf::Model &model, tinygltf::Accessor &accessor, const std::function<void(uint32_t, std::vector<ComponentType>, int)> &callback);

    template <typename T>
    std::optional<T> TryGetScalar(const std::vector<ComponentType> &accessor_value, int accessor_type);
    
    template <glm::length_t L, typename T>
    std::optional<glm::vec<L, T>> TryGetVector(const std::vector<ComponentType> &accessor_value, int accessor_type);

    template <glm::length_t C, glm::length_t R, typename T>
    std::optional<glm::mat<C, R, T>> TryGetMatrix(const std::vector<ComponentType> &accessor_value, int accessor_type);
private:
    static VkFilter GetVulkanFilter(int filter);
    static VkSamplerMipmapMode GetVulkanMipmapMode(int min_filter);
    static bool UseMipMaps(int min_filter);
    static VkSamplerAddressMode GetVulkanWrapMode(int wrap_mode);

    uint32_t GetComponentSize(int component_type);
    uint32_t GetAccessorTypeCount(int accessor_type);
    uint32_t GetStrideSize(int accessor_type, int component_type);
};

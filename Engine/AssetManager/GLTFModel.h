#pragma once

#include "Core/Handle.h"
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
#include "AssetManager/AssetManager.h"

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
        uint32_t index = 0;
        std::string name;
    
        Node *parent = nullptr;
        std::vector<std::shared_ptr<Node>> children {};
        std::optional<Mesh> mesh = std::nullopt;

        glm::vec3 translation {};
        glm::quat rotation { 1.0, 0.0, 0.0, 0.0 };
        glm::vec3 scale { 1.0 };
        glm::mat4 matrix { 1.0 };

        std::tuple<glm::vec3, glm::quat, glm::vec3> CalculateTRS() const;
    };

    struct Material {
        MaterialHandle handle;

        // For MikkTSpace
        bool has_normal_texture = false;
        uint32_t normal_texcoord = 0;
    };
public:
    GLTFModel(const VulkanDevice &device, AssetManager &asset_manager, const std::filesystem::path &file_path);
    ~GLTFModel();

    void GetCameras();
    
    // void PlayAnimation(uint32_t index, float time, bool loop = false);
    // void ResetPositions();

    void ForEachNode(const std::function<void(Node &)> &callback);
    void ForEachNode(const std::function<void(const Node &)> &callback) const;
    const VulkanBuffer &VertexBuffer() const { return *m_vertex_buffer; }
    const VulkanBuffer &IndexBuffer() const { return *m_index_buffer; }

    uint32_t NodeCount() const { return static_cast<uint32_t>(m_linear_nodes.size()); }
    const Node &GetNode(uint32_t index) const;
private:
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
        TextureHandle srgb_texture = TextureHandle::Invalid();
        TextureHandle unorm_texture = TextureHandle::Invalid();

        SamplerHandle sampler = SamplerHandle::Invalid();

        // Whether this texture needs an sRGB and/or UNORM gpu representation for this GLTF file
        bool needs_srgb = false;
        bool needs_unorm = false;
    };

    enum class AnimationTarget : uint8_t {
        None = 0,
        Weights,
        Translation,
        Rotation,
        Scale,
    };

    struct AnimationChannel {
        uint32_t sampler_index = 0;
        uint32_t node_index = 0;
        AnimationTarget target = AnimationTarget::None;
    };
    
    enum class AnimationInterpolation : uint8_t {
        Linear = 0,
        Step,
        CubicSpline
    };

    using AnimationOutput = std::variant<glm::vec3, glm::quat, float>;
    struct AnimationSampler {
        AnimationInterpolation interpolation_method = AnimationInterpolation::Linear;
        
        std::vector<float> times {};
        std::vector<AnimationOutput> output {};

        AnimationOutput Sample(float current_time) const;
    };

    struct Animation {
        std::string name {};
        std::vector<AnimationChannel> channels {};
        std::vector<AnimationSampler> samplers {};

		float duration = 0.0f;
    };
private:
    using ComponentType = std::variant<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float>;
private:
    const VulkanDevice &m_device;
    AssetManager &m_asset_manager;

    struct VertexData {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
    };
    
    std::vector<std::shared_ptr<Node>> m_root_nodes {};

    // Flattened array of all nodes
    std::vector<std::shared_ptr<Node>> m_linear_nodes {};

    std::vector<Texture> m_textures {};
    std::vector<Material> m_materials {};
    std::vector<Animation> m_animations {};

    std::unique_ptr<VulkanBuffer> m_vertex_buffer;
    std::unique_ptr<VulkanBuffer> m_index_buffer;
private:
    void LoadMaterials(tinygltf::Model &model);
    void LoadNode(Node *parent, uint32_t node_index, tinygltf::Model &model, VertexData &data);
    void LoadAnimations(tinygltf::Model &model);

    void GenerateTangents(std::vector<MeshVertex> &primitive_vertices, std::vector<uint32_t> &primitive_indices, uint32_t uv_set);

    // Callback runs over (index, accessor_value, accessor_value_type)
    void IterateAccessor(tinygltf::Model &model, tinygltf::Accessor &accessor, const std::function<void(uint32_t index, std::vector<ComponentType> value, int accessor_type)> &callback);

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

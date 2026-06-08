#pragma once

#include "descriptor_allocator.h"
#include "gpu_structs.h"
#include "renderable.h"
#include "vulkan_renderer.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

struct MeshPrimitive {
    uint32_t start_index;
    uint32_t index_count;

    VkDescriptorSet material_descriptor_set;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;

    MaterialPass pass;
};

struct GLTFMeshAsset {
    std::vector<MeshPrimitive> primitives;
    GPUMesh mesh_buffers;
};

// Fixed data used by all GLTF instances
struct GLTFCoreData {
    VkPipeline opaque_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout opaque_pipeline_layout = VK_NULL_HANDLE;
    
    VkPipeline transparent_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout transparent_pipeline_layout = VK_NULL_HANDLE;

    VkDescriptorSetLayout material_layout = VK_NULL_HANDLE;

    GPUImage default_texture;
    VkSampler default_sampler = VK_NULL_HANDLE;
};

struct GLTFMaterialData {
    glm::vec4 color_factors;
    glm::vec4 metallic_roughness_factors;
    //padding
    glm::vec4 extra[14];
};

struct GLTFMaterialResources {
    VkDescriptorSet descriptor_set;

    std::shared_ptr<GPUImage> color_image;
    std::shared_ptr<GPUImage> metallic_roughness_image;

    VkBuffer uniform_buffer;
    VmaAllocation uniform_buffer_alloc;
    MaterialPass pass;
};

struct GLTFSceneNode {
    std::shared_ptr<GLTFMeshAsset> mesh = nullptr;

    std::weak_ptr<GLTFSceneNode> parent;
    std::vector<std::shared_ptr<GLTFSceneNode>> children;

    glm::mat4 local_transform;
    glm::mat4 world_transform;
};

class GLTFModel : public IRenderable {
public:
    GLTFModel(const VulkanRenderer &renderer, const GLTFCoreData &core, std::filesystem::path file_path);

    void Load(std::filesystem::path file_path);
    void CleanUp();
    const std::vector<std::shared_ptr<GLTFMeshAsset>> &GetMeshes() const { return m_meshes; }

    virtual void Draw(DrawContext &context) override;
private:
    const VulkanRenderer &m_renderer;
    const GLTFCoreData &m_core;

    std::vector<std::shared_ptr<GLTFMeshAsset>> m_meshes;
    std::vector<VkSampler> m_samplers;
    std::vector<std::shared_ptr<GPUImage>> m_images;
    std::vector<GLTFMaterialResources> m_material_data;

    // Note: May not match the same order as tinygltf nodes in-file!
    std::vector<std::shared_ptr<GLTFSceneNode>> m_nodes;

    std::unique_ptr<DescriptorAllocator> m_descriptor_allocator;
private:
    void LoadNode(const tinygltf::Model &model, const tinygltf::Node &node, std::shared_ptr<GLTFSceneNode> scene_node);

    glm::mat4 GetNodeTransform(const tinygltf::Node &node);
    std::pair<VkFilter, VkSamplerMipmapMode> GetVulkanTextureFilters(int gltf_filter);
    VkSamplerAddressMode GetVulkanAddressMode(int gltf_wrapping);

    template <typename T>
    static T ReadScalar(const uint8_t *data, int component_type) {
        switch (component_type) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                return static_cast<T>(*reinterpret_cast<const uint8_t *>(data));
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                return static_cast<T>(*reinterpret_cast<const uint16_t *>(data));
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                return static_cast<T>(*reinterpret_cast<const uint32_t *>(data));
            case TINYGLTF_COMPONENT_TYPE_BYTE:
                return static_cast<T>(*reinterpret_cast<const int8_t *>(data));
            case TINYGLTF_COMPONENT_TYPE_SHORT:
                return static_cast<T>(*reinterpret_cast<const int16_t *>(data));
            case TINYGLTF_COMPONENT_TYPE_INT:
                return static_cast<T>(*reinterpret_cast<const int32_t *>(data));
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                return static_cast<T>(*reinterpret_cast<const float *>(data));
            default:
                return T{};
        }
    }

    template <typename ElementType>
    void IterateAccessor(const tinygltf::Model &model,
                        const tinygltf::Accessor &accessor,
                        std::function<void(uint32_t, ElementType)> callback)
    {
        const tinygltf::BufferView &buffer_view = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer &buffer = model.buffers[buffer_view.buffer];

        const size_t stride = accessor.ByteStride(buffer_view) ?
                            accessor.ByteStride(buffer_view) :
                            (std::is_arithmetic_v<ElementType>
                                ? tinygltf::GetComponentSizeInBytes(accessor.componentType)
                                : sizeof(ElementType));

        const uint8_t *base = &buffer.data[buffer_view.byteOffset + accessor.byteOffset];

        auto read_dense = [&](uint32_t i) -> ElementType {
            if constexpr (std::is_arithmetic_v<ElementType>)
                return ReadScalar<ElementType>(base + i * stride, accessor.componentType);
            else
                return *reinterpret_cast<const ElementType *>(base + i * stride);
        };

        if (!accessor.sparse.isSparse) {
            for (uint32_t i = 0; i < accessor.count; ++i)
                callback(i, read_dense(i));
            return;
        }

        const auto &sparse = accessor.sparse;

        const tinygltf::BufferView &sparse_index_buffer_view = model.bufferViews[sparse.indices.bufferView];
        const tinygltf::Buffer &sparse_index_buffer = model.buffers[sparse_index_buffer_view.buffer];
        const uint8_t *index_data = &sparse_index_buffer.data[sparse_index_buffer_view.byteOffset + sparse.indices.byteOffset];

        const tinygltf::BufferView &value_buffer_view = model.bufferViews[sparse.values.bufferView];
        const tinygltf::Buffer &value_buffer = model.buffers[value_buffer_view.buffer];
        const uint8_t *val_base = &value_buffer.data[value_buffer_view.byteOffset + sparse.values.byteOffset];

        uint32_t sparse_cursor = 0;

        for (uint32_t i = 0; i < accessor.count; ++i) {
            uint32_t next_sparse = (sparse_cursor < sparse.count)
                ? ReadScalar<uint32_t>(
                    index_data + sparse_cursor *
                    tinygltf::GetComponentSizeInBytes(sparse.indices.componentType),
                    sparse.indices.componentType)
                : UINT32_MAX;

            if (i == next_sparse) {
                ElementType val;
                if constexpr (std::is_arithmetic_v<ElementType>) {
                    val = ReadScalar<ElementType>(
                        val_base + sparse_cursor * tinygltf::GetComponentSizeInBytes(accessor.componentType),
                        accessor.componentType
                    );
                }
                else {
                    val = *reinterpret_cast<const ElementType *>(
                        val_base + sparse_cursor * sizeof(ElementType));

                }

                callback(i, val);
                ++sparse_cursor;
            }
            else {
                callback(i, read_dense(i));
            }
        }
    }
};

#pragma once

#include "mesh.h"
#include "vulkan_renderer.h"
#include <cstdint>
#include <filesystem>
#include <vector>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

struct MeshPrimitive {
    uint32_t start_index;
    uint32_t index_count;
};

struct GLTFMeshAsset {
    std::vector<MeshPrimitive> primitives;
    GPUMesh mesh_buffers;
};

class GLTFModel {
public:
    GLTFModel(const VulkanRenderer &renderer, std::filesystem::path file_path);

    void Load(std::filesystem::path file_path);
    void CleanUp();
    const std::vector<std::shared_ptr<GLTFMeshAsset>> &GetMeshes() const { return m_meshes; }
private:
    const VulkanRenderer &m_renderer;
    std::vector<std::shared_ptr<GLTFMeshAsset>> m_meshes;
    // std::vector<MeshVertex> m_vertices;
    // std::vector<uint32_t> m_indices;

private:
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

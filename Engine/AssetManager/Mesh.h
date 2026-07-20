#pragma once

#include "Material.h"
#include <glm/glm.hpp>

struct MeshVertex {
    glm::vec3 position {};
    glm::vec3 normal {};
    glm::vec4 tangent {};
    glm::vec2 uv0 {};
    glm::vec2 uv1 {};
    glm::vec4 color = glm::vec4(1.0f);
    glm::uvec2 joints0 {}; // 4 shorts packed
    glm::vec4 weights0 {};
};

class AssetManager;
class Mesh {
public:
    Mesh(AssetManager &asset_manager, const std::vector<MeshVertex> &vertices, const std::vector<uint32_t> &indices, const std::string &debug_name = "");
    ~Mesh();

    const VulkanBuffer &VertexBuffer() const;
    const VulkanBuffer &IndexBuffer() const;
    uint32_t IndexCount() const { return m_index_count; }
private:
    AssetManager &m_assets;

    BufferHandle m_vertex_buffer {};
    BufferHandle m_index_buffer {};

    uint32_t m_index_count = 0;
};

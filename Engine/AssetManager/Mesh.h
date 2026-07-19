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

class Mesh {
public:
    Mesh(const VulkanDevice &device, const std::vector<MeshVertex> &vertices, const std::vector<uint32_t> &indices);
    ~Mesh();

    const VulkanBuffer &VertexBuffer() const { return *m_vertex_buffer; }
    const VulkanBuffer &IndexBuffer() const { return *m_index_buffer; }
    uint32_t IndexCount() const { return m_index_count; }
private:
    std::unique_ptr<VulkanBuffer> m_vertex_buffer {};
    std::unique_ptr<VulkanBuffer> m_index_buffer {};

    uint32_t m_index_count = 0;
};

#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <vector>

struct ShapeVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 uv;
};

class Shape {
public:
    Shape() = default;
    Shape(const std::vector<ShapeVertex> &vertices, const std::vector<uint32_t> &indices);

    const std::vector<ShapeVertex> &Vertices() const { return m_vertices; }
    const std::vector<uint32_t> &Indices() const { return m_indices; }

    size_t VertexCount() const { return m_vertices.size(); }
    size_t IndexCount() const { return m_indices.size(); }

    static Shape UVSphere(float radius = 1.0f, uint32_t longitude_steps = 32, uint32_t latitude_steps = 16);
    static Shape Cube(float size = 1.0f);
    static Shape Cylinder(float radius = 0.5f, float height = 1.0f, uint32_t vertices = 32);
    static Shape Cone(float radius = 0.5f, float height = 1.0f, uint32_t vertices = 32);
    static Shape Quad(float width = 1.0f, float height = 1.0f);

    static Shape UVShape(uint32_t u_steps, uint32_t v_steps, const std::function<glm::vec3(float u, float v)> &compute_position);
private:
    std::vector<ShapeVertex> m_vertices {};
    std::vector<uint32_t> m_indices {};
};

#include "Shape.h"
#include <glm/ext/scalar_constants.hpp>
#include <numbers>

Shape::Shape(const std::vector<ShapeVertex> &vertices, const std::vector<uint32_t> &indices)
    : m_vertices(vertices)
    , m_indices(indices)
{}

Shape Shape::Cone(float radius, float height, uint32_t segments) {
    const auto compute_position = [&](float theta) -> glm::vec3 {
        return radius * glm::vec3(std::cos(theta), 0.0f, std::sin(theta));
    };

    const auto compute_normal = [&](float theta) -> glm::vec3 {
        auto normal = glm::vec3(height * std::cos(theta), radius, height * std::sin(theta));
        return glm::normalize(normal);
    };

    const auto compute_tangent = [&](float theta) -> glm::vec4 {
        return glm::vec4(-std::sin(theta), 0.0f, std::cos(theta), -1.0f);
    };

    std::vector<ShapeVertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(6 * static_cast<size_t>(segments));
    indices.reserve(6 * static_cast<size_t>(segments));
    
    glm::vec3 apex = glm::vec3(0.0f, height, 0.0f);

    for (uint32_t u = 0; u < segments; ++u) {
        //// Sides ////

        float u0 = static_cast<float>(u) / static_cast<float>(segments);
        float u1 = static_cast<float>(u + 1) / static_cast<float>(segments);
        
        float theta0 = u0 * 2.0f * glm::pi<float>();
        float theta1 = u1 * 2.0f * glm::pi<float>();

        auto current_vertex = static_cast<uint32_t>(vertices.size());

        vertices.push_back(ShapeVertex{
            .position = apex,
            .normal = compute_normal((theta0 + theta1) * 0.5f),
            .tangent = compute_tangent((theta0 + theta1) / 2.0f),
            .uv = glm::vec2((u0 + u1) / 2.0f, 1.0f),
        });

        vertices.push_back(ShapeVertex{
            .position = compute_position(theta0),
            .normal = compute_normal(theta0),
            .tangent = compute_tangent(theta0),
            .uv = glm::vec2(u0, 0.0f),
        });

        vertices.push_back(ShapeVertex{
            .position = compute_position(theta1),
            .normal = compute_normal(theta1),
            .tangent = compute_tangent(theta1),
            .uv = glm::vec2(u1, 0.0f),
        });

        indices.push_back(current_vertex);
        indices.push_back(current_vertex + 2);
        indices.push_back(current_vertex + 1);

        //// Bottom ////
        current_vertex = static_cast<uint32_t>(vertices.size());

        vertices.push_back(ShapeVertex{
            .position = glm::vec3(0.0f),
            .normal = glm::vec3(0.0f, -1.0f, 0.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .uv = glm::vec2(0.5f, 0.5f),
        });

        vertices.push_back(ShapeVertex{
            .position = compute_position(theta0),
            .normal = glm::vec3(0.0f, -1.0f, 0.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .uv = glm::vec2(std::cos(theta0) * 0.5f + 0.5f, std::sin(theta0) * 0.5f + 0.5f),
        });

        vertices.push_back(ShapeVertex{
            .position = compute_position(theta1),
            .normal = glm::vec3(0.0f, -1.0f, 0.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .uv = glm::vec2(std::cos(theta1) * 0.5f + 0.5f, std::sin(theta1) * 0.5f + 0.5f),
        });

        indices.push_back(current_vertex);
        indices.push_back(current_vertex + 1);
        indices.push_back(current_vertex + 2);
    }
    
    return Shape(vertices, indices);
}

Shape Shape::UVSphere(float radius, uint32_t longitude_steps, uint32_t latitude_steps) {
    std::vector<ShapeVertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(
        static_cast<size_t>(longitude_steps + 1) *
        static_cast<size_t>(latitude_steps + 1)
    );
    indices.reserve(
        6 * static_cast<size_t>(longitude_steps) *
        static_cast<size_t>(latitude_steps)
    );

    for (uint32_t latitude = 0; latitude <= latitude_steps; ++latitude) {
        float latitude_normalized =
            static_cast<float>(latitude) /
            static_cast<float>(latitude_steps);

        float phi = latitude_normalized * glm::pi<float>();
        float v = 1.0f - latitude_normalized;

        for (uint32_t longitude = 0; longitude <= longitude_steps; ++longitude) {
            float u =
                static_cast<float>(longitude) /
                static_cast<float>(longitude_steps);

            float theta = u * 2.0f * glm::pi<float>();

            glm::vec3 normal = glm::vec3(
                std::sin(phi) * std::cos(theta),
                std::cos(phi),
                std::sin(phi) * std::sin(theta)
            );

            glm::vec4 tangent = glm::vec4(
                -std::sin(theta),
                0.0f,
                std::cos(theta),
                -1.0f
            );

            vertices.push_back(ShapeVertex {
                .position = radius * normal,
                .normal = normal,
                .tangent = tangent,
                .uv = glm::vec2(u, v),
            });
        }
    }

    uint32_t stride = longitude_steps + 1;

    for (uint32_t latitude = 0; latitude < latitude_steps; ++latitude) {
        for (uint32_t longitude = 0; longitude < longitude_steps; ++longitude) {
            uint32_t current = latitude * stride + longitude;
            uint32_t next_u = current + 1;
            uint32_t next_v = current + stride;
            uint32_t next_uv = next_v + 1;

            if (latitude == 0) {
                indices.push_back(current);
                indices.push_back(next_uv);
                indices.push_back(next_v);
            }
            else if (latitude == latitude_steps - 1) {
                indices.push_back(current);
                indices.push_back(next_u);
                indices.push_back(next_v);
            }
            else {
                indices.push_back(current);
                indices.push_back(next_u);
                indices.push_back(next_v);

                indices.push_back(next_u);
                indices.push_back(next_uv);
                indices.push_back(next_v);
            }
        }
    }

    return Shape(vertices, indices);
}

Shape Shape::Cube(float size) {
    std::vector<ShapeVertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(24);
    indices.reserve(36);

    float half_size = size / 2.0f;

    const auto add_face = [&](glm::vec3 center, glm::vec3 right, glm::vec3 up) {
        glm::vec3 normal = glm::normalize(glm::cross(right, up));
        glm::vec4 tangent = glm::vec4(glm::normalize(right), 1.0f);

        auto current_vertex = static_cast<uint32_t>(vertices.size());

        vertices.push_back(ShapeVertex {
            .position = center - right - up,
            .normal = normal,
            .tangent = tangent,
            .uv = glm::vec2(0.0f, 0.0f),
        });

        vertices.push_back(ShapeVertex {
            .position = center + right - up,
            .normal = normal,
            .tangent = tangent,
            .uv = glm::vec2(1.0f, 0.0f),
        });

        vertices.push_back(ShapeVertex {
            .position = center - right + up,
            .normal = normal,
            .tangent = tangent,
            .uv = glm::vec2(0.0f, 1.0f),
        });

        vertices.push_back(ShapeVertex {
            .position = center + right + up,
            .normal = normal,
            .tangent = tangent,
            .uv = glm::vec2(1.0f, 1.0f),
        });

        indices.push_back(current_vertex);
        indices.push_back(current_vertex + 1);
        indices.push_back(current_vertex + 2);

        indices.push_back(current_vertex + 1);
        indices.push_back(current_vertex + 3);
        indices.push_back(current_vertex + 2);
    };

    add_face(
        glm::vec3(0.0f, 0.0f, half_size),
        glm::vec3(half_size, 0.0f, 0.0f),
        glm::vec3(0.0f, half_size, 0.0f)
    );

    add_face(
        glm::vec3(0.0f, 0.0f, -half_size),
        glm::vec3(-half_size, 0.0f, 0.0f),
        glm::vec3(0.0f, half_size, 0.0f)
    );

    add_face(
        glm::vec3(half_size, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -half_size),
        glm::vec3(0.0f, half_size, 0.0f)
    );

    add_face(
        glm::vec3(-half_size, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, half_size),
        glm::vec3(0.0f, half_size, 0.0f)
    );

    add_face(
        glm::vec3(0.0f, half_size, 0.0f),
        glm::vec3(half_size, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -half_size)
    );

    add_face(
        glm::vec3(0.0f, -half_size, 0.0f),
        glm::vec3(half_size, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, half_size)
    );

    return Shape(vertices, indices);
}

Shape Shape::Cylinder(float radius, float height, uint32_t segments) {
    const auto compute_position = [&](float theta, float y) -> glm::vec3 {
        return glm::vec3(
            radius * std::cos(theta),
            y,
            radius * std::sin(theta)
        );
    };

    const auto compute_normal = [&](float theta) -> glm::vec3 {
        return glm::vec3(std::cos(theta), 0.0f, std::sin(theta));
    };

    const auto compute_tangent = [&](float theta) -> glm::vec4 {
        return glm::vec4(-std::sin(theta), 0.0f, std::cos(theta), -1.0f);
    };

    std::vector<ShapeVertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(10 * static_cast<size_t>(segments));
    indices.reserve(12 * static_cast<size_t>(segments));

    for (uint32_t u = 0; u < segments; ++u) {
        float u0 = static_cast<float>(u) / static_cast<float>(segments);
        float u1 = static_cast<float>(u + 1) / static_cast<float>(segments);

        float theta0 = u0 * 2.0f * glm::pi<float>();
        float theta1 = u1 * 2.0f * glm::pi<float>();

        glm::vec3 bottom0 = compute_position(theta0, 0.0f);
        glm::vec3 bottom1 = compute_position(theta1, 0.0f);
        glm::vec3 top0 = compute_position(theta0, height);
        glm::vec3 top1 = compute_position(theta1, height);

        //// Sides ////

        auto current_vertex = static_cast<uint32_t>(vertices.size());

        vertices.push_back(ShapeVertex {
            .position = top0,
            .normal = compute_normal(theta0),
            .tangent = compute_tangent(theta0),
            .uv = glm::vec2(u0, 1.0f),
        });

        vertices.push_back(ShapeVertex {
            .position = bottom1,
            .normal = compute_normal(theta1),
            .tangent = compute_tangent(theta1),
            .uv = glm::vec2(u1, 0.0f),
        });

        vertices.push_back(ShapeVertex {
            .position = bottom0,
            .normal = compute_normal(theta0),
            .tangent = compute_tangent(theta0),
            .uv = glm::vec2(u0, 0.0f),
        });

        vertices.push_back(ShapeVertex {
            .position = top0,
            .normal = compute_normal(theta0),
            .tangent = compute_tangent(theta0),
            .uv = glm::vec2(u0, 1.0f),
        });

        vertices.push_back(ShapeVertex {
            .position = top1,
            .normal = compute_normal(theta1),
            .tangent = compute_tangent(theta1),
            .uv = glm::vec2(u1, 1.0f),
        });

        vertices.push_back(ShapeVertex {
            .position = bottom1,
            .normal = compute_normal(theta1),
            .tangent = compute_tangent(theta1),
            .uv = glm::vec2(u1, 0.0f),
        });

        for (uint32_t i = 0; i < 6; ++i)
            indices.push_back(current_vertex + i);

        //// Top ////

        current_vertex = static_cast<uint32_t>(vertices.size());

        vertices.push_back(ShapeVertex {
            .position = glm::vec3(0.0f, height, 0.0f),
            .normal = glm::vec3(0.0f, 1.0f, 0.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, -1.0f),
            .uv = glm::vec2(0.5f, 0.5f),
        });

        vertices.push_back(ShapeVertex {
            .position = top1,
            .normal = glm::vec3(0.0f, 1.0f, 0.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, -1.0f),
            .uv = glm::vec2(
                std::cos(theta1) * 0.5f + 0.5f,
                std::sin(theta1) * 0.5f + 0.5f
            ),
        });

        vertices.push_back(ShapeVertex {
            .position = top0,
            .normal = glm::vec3(0.0f, 1.0f, 0.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, -1.0f),
            .uv = glm::vec2(
                std::cos(theta0) * 0.5f + 0.5f,
                std::sin(theta0) * 0.5f + 0.5f
            ),
        });

        indices.push_back(current_vertex);
        indices.push_back(current_vertex + 1);
        indices.push_back(current_vertex + 2);

        //// Bottom ////

        current_vertex = static_cast<uint32_t>(vertices.size());

        vertices.push_back(ShapeVertex {
            .position = glm::vec3(0.0f),
            .normal = glm::vec3(0.0f, -1.0f, 0.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .uv = glm::vec2(0.5f, 0.5f),
        });

        vertices.push_back(ShapeVertex {
            .position = bottom0,
            .normal = glm::vec3(0.0f, -1.0f, 0.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .uv = glm::vec2(
                std::cos(theta0) * 0.5f + 0.5f,
                std::sin(theta0) * 0.5f + 0.5f
            ),
        });

        vertices.push_back(ShapeVertex {
            .position = bottom1,
            .normal = glm::vec3(0.0f, -1.0f, 0.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .uv = glm::vec2(
                std::cos(theta1) * 0.5f + 0.5f,
                std::sin(theta1) * 0.5f + 0.5f
            ),
        });

        indices.push_back(current_vertex);
        indices.push_back(current_vertex + 1);
        indices.push_back(current_vertex + 2);
    }

    return Shape(vertices, indices);
}

Shape Shape::Quad(float width, float height) {
    float half_width = width / 2.0f;
    float half_height = height / 2.0f;

    std::vector<ShapeVertex> vertices {
        ShapeVertex {
            .position = glm::vec3(-half_width, -half_height, 0.0f),
            .normal = glm::vec3(0.0f, 0.0f, 1.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .uv = glm::vec2(0.0f, 0.0f),
        },
        ShapeVertex {
            .position = glm::vec3(half_width, -half_height, 0.0f),
            .normal = glm::vec3(0.0f, 0.0f, 1.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .uv = glm::vec2(1.0f, 0.0f),
        },
        ShapeVertex {
            .position = glm::vec3(-half_width, half_height, 0.0f),
            .normal = glm::vec3(0.0f, 0.0f, 1.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .uv = glm::vec2(0.0f, 1.0f),
        },
        ShapeVertex {
            .position = glm::vec3(half_width, half_height, 0.0f),
            .normal = glm::vec3(0.0f, 0.0f, 1.0f),
            .tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .uv = glm::vec2(1.0f, 1.0f),
        },
    };

    std::vector<uint32_t> indices {
        0, 1, 2,
        1, 3, 2,
    };

    return Shape(vertices, indices);
}
Shape Shape::UVShape(
    uint32_t u_steps,
    uint32_t v_steps,
    const std::function<glm::vec3(float u, float v)> &compute_position
) {
    std::vector<ShapeVertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(
        static_cast<size_t>(u_steps + 1) *
        static_cast<size_t>(v_steps + 1)
    );

    indices.reserve(
        6 * static_cast<size_t>(u_steps) *
        static_cast<size_t>(v_steps)
    );

    constexpr float epsilon = 0.0001f;

    for (uint32_t v = 0; v <= v_steps; ++v) {
        float normalized_v =
            static_cast<float>(v) /
            static_cast<float>(v_steps);

        for (uint32_t u = 0; u <= u_steps; ++u) {
            float normalized_u =
                static_cast<float>(u) /
                static_cast<float>(u_steps);

            float u0 = std::max(normalized_u - epsilon, 0.0f);
            float u1 = std::min(normalized_u + epsilon, 1.0f);
            float v0 = std::max(normalized_v - epsilon, 0.0f);
            float v1 = std::min(normalized_v + epsilon, 1.0f);

            glm::vec3 tangent = glm::normalize(
                compute_position(u1, normalized_v) -
                compute_position(u0, normalized_v)
            );

            glm::vec3 bitangent = glm::normalize(
                compute_position(normalized_u, v1) -
                compute_position(normalized_u, v0)
            );

            glm::vec3 normal = glm::normalize(glm::cross(tangent, bitangent));

            vertices.push_back(ShapeVertex {
                .position = compute_position(normalized_u, normalized_v),
                .normal = normal,
                .tangent = glm::vec4(tangent, 1.0f),
                .uv = glm::vec2(normalized_u, normalized_v),
            });
        }
    }

    uint32_t stride = u_steps + 1;

    for (uint32_t v = 0; v < v_steps; ++v) {
        for (uint32_t u = 0; u < u_steps; ++u) {
            uint32_t current = v * stride + u;
            uint32_t next_u = current + 1;
            uint32_t next_v = current + stride;
            uint32_t next_uv = next_v + 1;

            indices.push_back(current);
            indices.push_back(next_u);
            indices.push_back(next_v);

            indices.push_back(next_u);
            indices.push_back(next_uv);
            indices.push_back(next_v);
        }
    }

    return Shape(vertices, indices);
}

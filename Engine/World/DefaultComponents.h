#pragma once

#include <deque>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <variant>
#include <vector>

#include "Core/Handle.h"
#include "Core/Math.h"

//// Transform ////

struct Transform {
    glm::vec3 translation {};
    glm::quat rotation { 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 scale { 1.0f };

    bool operator==(const Transform &other) const {
        return NearlyEqual(translation, other.translation)
            && NearlyEqual(rotation, other.rotation)
            && NearlyEqual(scale, other.scale);
    }

    static Transform FromMatrix(const glm::mat4 &matrix);
    glm::mat4 CalculateLocalMatrix() const;
};

//// Camera ////

struct PerspectiveProjection {
    float fov = glm::radians(60.0f);
    float near_plane = 0.05f;
    float far_plane = 1000.0f;

    glm::mat4 CalculateProjectionMatrix(float aspect) const;
};

struct OrthographicProjection {
    float vertical_size = 10.0f;
    float near_plane = 0.05f;
    float far_plane = 1000.0f;

    glm::mat4 CalculateProjectionMatrix(float aspect) const;
};

using CameraComponent = std::variant<PerspectiveProjection, OrthographicProjection>;
glm::mat4 CalculateProjectionMatrix(const CameraComponent &camera_component, float aspect);

//// Mesh ////

struct ProceduralMeshComponent {
    MeshHandle mesh = MeshHandle::Invalid();
    MaterialHandle material = MaterialHandle::Invalid();
    bool visible = true;
};

struct GLTFMeshComponent {
    GLTFHandle gltf = GLTFHandle::Invalid();
    uint32_t node_index = 0;
    bool visible = false;
};

//// Lights ////

struct DirectionalLight {
    glm::vec4 color = glm::vec4(1.0f);
    float intensity = 1.0f;
};

struct PointLight {
    glm::vec4 color = glm::vec4(1.0f);
    float intensity = 1.0f;
    float range = 10.0f;
};

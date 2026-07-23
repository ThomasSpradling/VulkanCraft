#include "DefaultComponents.h"
#include "Core/errors.h"
#include <variant>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

Transform Transform::FromMatrix(const glm::mat4 &matrix) {
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(matrix, scale, rotation, translation, skew, perspective);

    return Transform {
        .translation = translation,
        .rotation = rotation,
        .scale = scale,
    };
}

glm::mat4 Transform::CalculateLocalMatrix() const {
    const glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), translation);
    const glm::mat4 rotation_matrix = glm::mat4_cast(rotation);
    const glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0f), scale);

    return translation_matrix * rotation_matrix * scale_matrix;
}

// ================ //
// ---- Camera ---- //
// ================ //
glm::mat4 PerspectiveProjection::CalculateProjectionMatrix(float aspect) const {
    glm::mat4 matrix = glm::perspective(fov, aspect, near_plane, far_plane);
    // matrix[1][1] *= -1.0f;
    return matrix;
}

glm::mat4 OrthographicProjection::CalculateProjectionMatrix(float aspect) const {        
    float half_height = vertical_size / 2.0f;
    float half_width  = half_height * aspect;
    glm::mat4 matrix = glm::ortho(-half_width, half_width, -half_height, half_height, near_plane, far_plane);
    // matrix[1][1] *= -1.0f;
    return matrix;
}


glm::mat4 CalculateProjectionMatrix(const CameraComponent &camera, float aspect) {
    if (const auto* persp = std::get_if<PerspectiveProjection>(&camera)) {
        return persp->CalculateProjectionMatrix(aspect);
    } else if (const auto *ortho = std::get_if<OrthographicProjection>(&camera)) {
       return ortho->CalculateProjectionMatrix(aspect);
    }

    Assert(false, "Cannot calculate projection matrix of non-valid camera!");
    return glm::mat4(1.0f);
}


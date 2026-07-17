#include "DefaultComponents.h"
#include "Core/errors.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

Transform Transform::FromMatrix(const glm::mat4 &matrix) {
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(matrix, scale, rotation, translation, skew, perspective);

    // Assert(skew == glm::vec3(0.0f) && perspective == glm::vec4(0.0f) && scale != glm::vec3(0.0f), "Invalid matrix decomposition!");

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

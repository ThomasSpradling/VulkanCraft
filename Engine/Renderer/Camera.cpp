// #include "Camera.h"
// #include <glm/ext/matrix_transform.hpp>
// #define GLM_ENABLE_EXPERIMENTAL
// #include <glm/gtx/transform.hpp>

// #include "Core/Math.h"

// void Camera::SetAspect(uint32_t width, uint32_t height) {
//     if (height == 0) {
//         m_aspect = 1.0f;
//         return;
//     }

//     m_aspect = static_cast<float>(width) / static_cast<float>(height);
// }

// void Camera::SetNearFar(float near, float far) {
//     m_near = near;
//     m_far = far;
// }

// void Camera::LookAt(glm::vec3 position) {
//     m_view = glm::normalize(position - m_position);
// }

// glm::mat4 Camera::ComputeProjectionMatrix() const {
//     auto projection = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
//     projection[1][1] *= -1.0f;
//     return projection;
// }

// glm::mat4 Camera::ComputeViewMatrix() const {
//     const auto view = glm::normalize(m_view);
//     auto up = glm::vec3(0.0f, 1.0f, 0.0f);

//     if (NearlyEqual(view, up) || NearlyEqual(view, -up)) {
//         up = glm::vec3(0.0f, 0.0f, 1.0f);
//     }

//     return glm::lookAt(m_position, m_position + view, up);
// }

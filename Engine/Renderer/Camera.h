#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    Camera() = default;

    glm::vec3 GetPosition() const { return m_position; }
    glm::vec3 GetViewDirection() const { return m_view; }
    void SetPosition(glm::vec3 position) { m_position = position; }
    void SetViewDirection(glm::vec3 view) { m_view = view; }

    void LookAt(glm::vec3 position);

    void SetFOV(float fov) { m_fov = fov; }
    void SetAspect(uint32_t width, uint32_t height);
    void SetNearFar(float near, float far);

    glm::mat4 ComputeProjectionMatrix() const;
    glm::mat4 ComputeViewMatrix() const;
private:
    glm::vec3 m_position;
    glm::vec3 m_view;

    float m_fov = 45.0f;
    float m_aspect = 45.0f;
    float m_near = 0.1f;
    float m_far = 1000.0f;
};
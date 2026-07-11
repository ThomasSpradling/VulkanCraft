#include "SimpleGame.h"
#include <iostream>

void SimpleGame::Initialize(ClientContext &context) {
    // context.renderer.EnableVSync();

    m_camera = &context.renderer.GetCamera();
    m_camera->SetViewDirection(glm::vec3(0.0f, 0.0f, -1.0f));
    m_camera->SetFOV(45.0f);

    m_player.position = glm::vec3(0.0f, 0.0f, 3.0f);
    m_player.yaw = -90.0f;
    m_player.pitch = 0.0f;
}

void SimpleGame::ShutDown(ClientContext &context) {
}

void SimpleGame::Update(double delta_time, ClientContext &context) {
    HandleInputs(delta_time, context.input_handler);
}

void SimpleGame::Render(double delta_time, ClientContext &context) {
    m_camera->SetPosition(m_player.position);
    m_camera->SetViewDirection(m_player.view_direction);
    context.renderer.DrawFrame();
}

void SimpleGame::HandleInputs(double delta_time, InputHandler &input_handler) {
    //// View Direction ////

    float mouse_delta_x = input_handler.MouseOffset().x;
    float mouse_delta_y = input_handler.MouseOffset().y;

    m_player.yaw   += mouse_delta_x * MouseSensitivity;
    m_player.pitch -= mouse_delta_y * MouseSensitivity;

    m_player.pitch = std::clamp(m_player.pitch, -89.0f, 89.0f);

    //// Position ////

    float yaw   = glm::radians(m_player.yaw);
    float pitch = glm::radians(m_player.pitch);

    glm::vec3 forward;
    forward.x = std::cos(yaw) * std::cos(pitch);
    forward.y = std::sin(pitch);
    forward.z = std::sin(yaw) * std::cos(pitch);

    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::cross(forward, up);

    glm::vec3 move_direction = glm::vec3(0.0f);
    if (input_handler.IsKeyDown(GLFW_KEY_W))
        move_direction += forward;
    if (input_handler.IsKeyDown(GLFW_KEY_A))
        move_direction -= right;
    if (input_handler.IsKeyDown(GLFW_KEY_S))
        move_direction -= forward;
    if (input_handler.IsKeyDown(GLFW_KEY_D))
        move_direction += right;

    if (glm::dot(move_direction, move_direction) > 0.0f)
        move_direction = glm::normalize(move_direction);

    m_player.position += move_direction * PlayerSpeed * static_cast<float>(delta_time) / 1000.0f;
    m_player.view_direction = forward;
}

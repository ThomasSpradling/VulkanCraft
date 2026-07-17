#include "SimpleGame.h"

#include "Application/Client/ClientContext.h"
#include "AssetManager/AssetManager.h"
#include "Platform/Window/InputHandler.h"
#include "Renderer/Renderer.h"
#include "World/DefaultComponents.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

void SimpleGame::Initialize(ClientContext &context) {
    // context.renderer.EnableVSync();

    GLTFHandle helmet = context.assets.LoadGLTF(ASSET_PATH "/models/SimpleSparseAccessor.gltf");
    context.world.BuildGLTF(helmet);

    CreatePlayer(context);

    m_camera = context.world.CreateEntity("Main Camera");
    context.world.Add<CameraComponent>(m_camera, PerspectiveProjection{
        .fov = glm::radians(45.0f),
        .near_plane = 0.05f,
        .far_plane = 350.0f,
    });

    context.world.AttachParent(m_camera, m_player.player, TransformMethod::Local);
}

void SimpleGame::CreatePlayer(ClientContext &context) {
    m_player.player = context.world.CreateEntity("Player");

    m_player.position = glm::vec3(0.0f, 0.0f, 3.0f);
    m_player.yaw = -90.0f;
    m_player.pitch = 0.0f;
    m_player.view_direction = glm::vec3(0.0f, 0.0f, -1.0f);

    auto &transform = context.world.Get<Transform>(m_player.player);

    transform.translation = m_player.position;
    transform.rotation = glm::quatLookAtRH(m_player.view_direction, glm::vec3(0.0f, 1.0f, 0.0f));
}

void SimpleGame::ShutDown(ClientContext &context) {
    if (context.world.IsAlive(m_player.player))
        context.world.DestroyEntityTree(m_player.player);

    m_camera = Entity::Invalid();
    m_player.player = Entity::Invalid();
}

void SimpleGame::Update(double delta_time, ClientContext &context) {
    HandleInputs(delta_time, context.input_handler);

    auto &player_transform = context.world.Get<Transform>(m_player.player);

    player_transform.translation = m_player.position;
    player_transform.rotation = glm::quatLookAtRH(
        glm::normalize(m_player.view_direction),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

void SimpleGame::Render(double delta_time, ClientContext &context) {
    context.renderer.RenderScene(context.world, m_camera, {});
}

void SimpleGame::HandleInputs(double delta_time, InputHandler &input_handler) {
    //// View Direction ////

    const glm::vec2 mouse_offset = input_handler.MouseOffset();
    m_player.yaw += mouse_offset.x * MouseSensitivity;
    m_player.pitch -= mouse_offset.y * MouseSensitivity;

    m_player.pitch = std::clamp(m_player.pitch, -89.0f, 89.0f);

    const float yaw = glm::radians(m_player.yaw);
    const float pitch = glm::radians(m_player.pitch);

    glm::vec3 forward {
        std::cos(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::sin(yaw) * std::cos(pitch),
    };

    forward = glm::normalize(forward);

    const glm::vec3 world_up { 0.0f, 1.0f, 0.0f };
    const glm::vec3 right = glm::normalize(glm::cross(forward, world_up));

    //// Position ////

    glm::vec3 move_direction { 0.0f };

    if (input_handler.IsKeyDown(GLFW_KEY_W))
        move_direction += forward;

    if (input_handler.IsKeyDown(GLFW_KEY_S))
        move_direction -= forward;

    if (input_handler.IsKeyDown(GLFW_KEY_A))
        move_direction -= right;

    if (input_handler.IsKeyDown(GLFW_KEY_D))
        move_direction += right;

    if (input_handler.IsKeyDown(GLFW_KEY_SPACE))
        move_direction += world_up;

    if (input_handler.IsKeyDown(GLFW_KEY_LEFT_SHIFT))
        move_direction -= world_up;

    if (glm::dot(move_direction, move_direction) > 0.0f) {
        move_direction = glm::normalize(move_direction);
    }

    const float delta_seconds = static_cast<float>(delta_time) / 1000.0f;
    m_player.position += move_direction * PlayerSpeed * delta_seconds;
    m_player.view_direction = forward;
}
#pragma once

#include "Application/Client/ClientContext.h"
#include <Engine/Client.h>

class SimpleGame : public IClientGame {
public:
    SimpleGame() = default;

    void Initialize(ClientContext &context) override;
    void ShutDown(ClientContext &context) override;
    void Update(double delta_time, ClientContext &context) override;
    void Render(double delta_time, ClientContext &context) override;
private:
    struct Player {
        glm::vec3 position;
        float yaw;
        float pitch;

        glm::vec3 view_direction;
        Entity player = Entity::Invalid();
    };
private:
    double m_time = 0.0f;

    Entity m_camera;
    // Entity m_sun;
    Player m_player;

    glm::vec3 light_direction = glm::vec3(-1.0f, -1.0f, 0.0f);

    const float MouseSensitivity = 0.2f;
    const float PlayerSpeed = 5.0f; // units / sec
private:
    void CreatePlayer(ClientContext &context);
    void HandleInputs(double delta_time, InputHandler &input_handler);
};

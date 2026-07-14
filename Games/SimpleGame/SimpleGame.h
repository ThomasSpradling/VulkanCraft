#pragma once

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
    };

    struct Models {
        std::unique_ptr<GLTFModel> model;
    };
private:
    Camera *m_camera;
    Player m_player;

    const float MouseSensitivity = 0.2f;
    const float PlayerSpeed = 5.0f; // units / sec
private:
    void HandleInputs(double delta_time, InputHandler &input_handler);
};

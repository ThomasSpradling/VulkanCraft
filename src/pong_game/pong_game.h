#pragma once
// #include "../game.h"
#include "../application.h"
#include "../events/event_listener.h"

class PongGame : public Game, public EventListener {
public:
    virtual void Initialize() override;
    virtual void ShutDown() override;
    virtual void Update(float delta_time) override;
    virtual void Render(const Frame &frame, float delta_time) override;
    virtual void OnEvent(const Event &event) override;
private:
    float m_money = 0.0f;
    bool m_claimed_prize = false;
    VkClearColorValue m_clear_color = { 164.0f/256.0f, 30.0f/256.0f, 34.0f/256.0f, 0.0f };
};

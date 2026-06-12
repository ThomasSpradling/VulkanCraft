#pragma once

#include <GLFW/glfw3.h>
#include <array>
#include <glm/glm.hpp>
#include <memory>

#include "../Events/EventHandler.h"

struct Button {
    bool pressed = false;
    bool down = false;
};

struct Keyboard {
    std::array<Button, GLFW_KEY_LAST> keys;
};

struct Mouse {
    std::array<Button, GLFW_MOUSE_BUTTON_LAST> buttons;
    glm::vec2 delta { 0.0f };
    glm::vec2 position { 0.0f };
    bool grabbed = false;

    struct Wheel {
        float offset_y = 0.0f;
    } wheel;
};

class InputHandler : public EventListener {
public:
    InputHandler(EventHandler &event_handler) : m_event_handler(event_handler) {}

    void Initialize();
    void ShutDown();

    virtual void OnEvent(const Event &event) override;

    bool IsKeyPressed(int key);
    inline bool IsKeyUp(int key) const { return !m_keyboard.keys.at(key).down; }
    inline bool IsKeyDown(int key) const { return m_keyboard.keys.at(key).down; };

    bool IsMouseButtonPressed(int button);
    inline bool IsMouseButtonUp(int button) const { return !m_mouse.buttons.at(button).down; }
    inline bool IsMouseButtonDown(int button) const { return m_mouse.buttons.at(button).down; };

    inline glm::vec2 GetMousePosition() const { return m_mouse.position; }
    inline float GetMousePositionX() const { return m_mouse.position.x; }
    inline float GetMousePositionY() const { return m_mouse.position.y; }

    float GetMouseDeltaX();
    float GetMouseDeltaY();

    inline float GetMouseWheelOffsetY() const { return m_mouse.wheel.offset_y; };
private:
    float m_mouse_sensitivity = 0.2f;

    Keyboard m_keyboard;
    Mouse m_mouse;
    EventHandler &m_event_handler;
};
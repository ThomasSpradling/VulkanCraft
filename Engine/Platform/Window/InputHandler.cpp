#include "InputHandler.h"
#include "Window.h"

InputHandler::InputHandler(Window &window)
    : m_window(window)
{}

void InputHandler::Update() {
    // Pressing should last only until the end of the frame, independent of the state of button.down

    for (auto &button : m_window.m_keyboard.keys)
        button.pressed = false;

    for (auto &button : m_window.m_mouse.buttons)
        button.pressed = false;
}

bool InputHandler::IsKeyPressed(int key) const {
    return m_window.m_keyboard.keys[key].pressed;
}

bool InputHandler::IsKeyDown(int key) const {
    return m_window.m_keyboard.keys[key].down;
}

bool InputHandler::IsKeyUp(int key) const {
    return !IsKeyDown(key);
}

bool InputHandler::IsMouseButtonPressed(int button) const {
    return m_window.m_mouse.buttons[button].pressed;
}

bool InputHandler::IsMouseButtonDown(int button) const {
    return m_window.m_mouse.buttons[button].down;
}

bool InputHandler::IsMouseButtonUp(int button) const {
    return !IsMouseButtonDown(button);
}

glm::vec2 InputHandler::MousePosition() const {
    return m_window.m_mouse.position;
}

glm::vec2 InputHandler::MouseOffset() const {
    return m_window.m_mouse.delta;
}

float InputHandler::ScrollOffset() const {
    return m_window.m_mouse.wheel.offset_y;
}

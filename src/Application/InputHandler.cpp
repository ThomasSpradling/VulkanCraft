#include "InputHandler.h"
#include <iostream>
#include "../Events/EventDispatcher.h"
#include "../Events/WindowEvents/KeyEvents.h"
#include "../Events/WindowEvents/MouseEvents.h"

void InputHandler::Initialize() {
    std::cout << "Initialized input handler." << std::endl;
    m_event_handler.AddListener(this);
}

void InputHandler::ShutDown() {
    std::cout << "Shutting down input handler." << std::endl;
    m_event_handler.RemoveListener(this);
}

void InputHandler::OnEvent(const Event &event) {
    EventDispatcher dispatcher(event);

    //// Keyboard Events ////
    dispatcher.Dispatch<KeyPressedEvent>([this](const KeyPressedEvent &e) {
        m_keyboard.keys[e.GetKeyCode()].pressed = true;
        m_keyboard.keys[e.GetKeyCode()].down = true;
    });

    dispatcher.Dispatch<KeyReleasedEvent>([this](const KeyReleasedEvent &e) {
        m_keyboard.keys[e.GetKeyCode()].down = false;
    });

    //// Mouse Button Events ////
    dispatcher.Dispatch<MouseButtonPressedEvent>([this](const MouseButtonPressedEvent &e) {
        m_mouse.buttons[e.GetMouseButton()].pressed = true;
        m_mouse.buttons[e.GetMouseButton()].down = true;
    });

    dispatcher.Dispatch<MouseButtonReleasedEvent>([this](const MouseButtonReleasedEvent &e) {
        m_mouse.buttons[e.GetMouseButton()].down = false;
    });

    dispatcher.Dispatch<MouseScrolledEvent>([this](const MouseScrolledEvent &e) {
        m_mouse.wheel.offset_y = e.GetOffsetY();
    });

    //// Mouse Movement Events ////
    dispatcher.Dispatch<MouseMovedEvent>([this](const MouseMovedEvent &e) {
        m_mouse.delta = e.GetMouseOffset();
        m_mouse.position = e.GetMousePosition();

        m_mouse.delta *= m_mouse_sensitivity;
    });
}

bool InputHandler::IsKeyPressed(int key) {
    bool is_pressed = m_keyboard.keys.at(key).pressed;
    m_keyboard.keys.at(key).pressed = false;
    return is_pressed;
}

bool InputHandler::IsMouseButtonPressed(int button) {
    bool is_pressed = m_mouse.buttons.at(button).pressed;
    m_mouse.buttons.at(button).pressed = false;
    return is_pressed;
}

float InputHandler::GetMouseDeltaX() {
    float delta = m_mouse.delta.x;
    m_mouse.delta.x = 0.0f;
    return delta;
}

float InputHandler::GetMouseDeltaY() {
    float delta = m_mouse.delta.y;
    m_mouse.delta.y = 0.0f;
    return delta;
}

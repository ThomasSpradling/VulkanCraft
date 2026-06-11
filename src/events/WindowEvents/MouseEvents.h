#pragma once

#include "../Event.h"
#include <glm/glm.hpp>

class MouseButtonPressedEvent : public Event {
public:
    MouseButtonPressedEvent(int button) : m_button(button) {}

    int GetMouseButton() const { return m_button; }

    DEFINE_EVENT_TYPE(MouseButtonPressedEvent)
private:
    int m_button;
};

class MouseButtonReleasedEvent : public Event {
public:
    MouseButtonReleasedEvent(int button) : m_button(button) {}

    int GetMouseButton() const { return m_button; }

    DEFINE_EVENT_TYPE(MouseButtonReleasedEvent)
private:
    int m_button;
};

class MouseMovedEvent : public Event {
public:
    MouseMovedEvent(glm::vec2 position, glm::vec2 offset)
        : m_position(position)
        , m_offset(offset)
    {}

    glm::vec2 GetMousePosition() const { return m_position; }
    glm::vec2 GetMouseOffset() const { return m_offset; }

    DEFINE_EVENT_TYPE(MouseMovedEvent)
private:
    glm::vec2 m_position, m_offset;
};

class MouseScrolledEvent : public Event {
public:
    MouseScrolledEvent(float y_offset) : m_y_offset(y_offset) {}

    float GetOffsetY() const { return m_y_offset; }

    DEFINE_EVENT_TYPE(MouseScrolledEvent)
private:
    float m_y_offset;
};

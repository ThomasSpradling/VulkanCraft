#pragma once

#include "../event.h"

class KeyPressedEvent : public Event {
public:
    KeyPressedEvent(int key, bool is_repeat)
        : m_key(key)
        , m_is_repeat(is_repeat)
    {}

    int GetKeyCode() const { return m_key; }
    bool IsRepeat() const { return m_is_repeat; }

    DEFINE_EVENT_TYPE(KeyPressedEvent)
private:
    int m_key;
    bool m_is_repeat;
};

class KeyReleasedEvent : public Event {
public:
    KeyReleasedEvent(int key) : m_key(key) {}

    int GetKeyCode() const { return m_key; }

    DEFINE_EVENT_TYPE(KeyReleasedEvent)
private:
    int m_key;
};

#pragma once

#include "event.h"

class EventDispatcher {
public:
    EventDispatcher(const Event &event) : m_event(event) {}

    template<typename E, typename F>
    bool Dispatch(const F &handler) {
        if (m_event.GetType() == E::GetStaticType()) {
            handler(static_cast<const E &>(m_event));
            return true;
        }
        return false;
    }
private:
    const Event &m_event;
};

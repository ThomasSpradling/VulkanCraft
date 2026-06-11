#include "EventHandler.h"

void EventHandler::AddListener(EventListener *listener) {
    m_listeners.push_back(listener);
}

void EventHandler::RemoveListener(EventListener *listener) {
    auto it = std::find(m_listeners.begin(), m_listeners.end(), listener);
    if (it != m_listeners.end()) {
        m_listeners.erase(it);
    }
}

void EventHandler::SignalEvent(const Event &event) {
    for (auto listener : m_listeners) {
        listener->OnEvent(event);
    }
}

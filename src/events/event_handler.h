#pragma once

#include "event_listener.h"
#include <memory>
#include <vector>
class EventHandler {
public:
    void AddListener(EventListener *listener);
    void RemoveListener(EventListener *listener);
    void SignalEvent(const Event &event);
private:
    std::vector<EventListener *> m_listeners; 
};
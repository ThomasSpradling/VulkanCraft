#pragma once

#include "event.h"

class EventListener {
public:
    virtual ~EventListener() = default;
    virtual void OnEvent(const Event &event) = 0;
};

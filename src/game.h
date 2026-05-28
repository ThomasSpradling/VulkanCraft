#pragma once

#include "events/event_handler.h"
#include "backend/resource_manager/resource_manager.h"
class Application;
#include "vulkan_renderer.h"
class Game {
public:
    Game() {}

    void SetUp(Application *application, std::shared_ptr<EventHandler> event_handler, std::shared_ptr<ResourceManager> resource_manager) {
        m_application = application;
        m_event_handler = event_handler;
        m_resource_manager = resource_manager;
        // m_application->GetWindowSize();
    }

    virtual void Initialize() = 0;
    virtual void ShutDown() = 0;

    // Handles the logical frame updating logic
    virtual void Update(float delta_time) = 0;

    // Handles one graphical frame of rendering
    virtual void Render(const Frame &frame, float delta_time) = 0;
protected:
    Application *m_application;
    std::shared_ptr<EventHandler> m_event_handler;
    std::shared_ptr<ResourceManager> m_resource_manager;
};

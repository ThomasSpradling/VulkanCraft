#pragma once

#include "Events/event_handler.h"
class Application;
#include "Graphics/vulkan_renderer.h"
class Game {
public:
    Game() {}

    void SetUp(Application *application, std::shared_ptr<EventHandler> event_handler, std::shared_ptr<VulkanRenderer> renderer) {
        m_application = application;
        m_event_handler = event_handler;
        m_renderer = renderer;
        // m_application->GetWindowSize();
    }

    virtual void Initialize() = 0;
    virtual void ShutDown() = 0;

    // Handles the logical frame updating logic
    virtual void Update(float delta_time) = 0;

    // Handles one graphical frame of rendering
    virtual void Render(std::optional<Frame> frame, float delta_time) = 0;
protected:
    Application *m_application;
    std::shared_ptr<EventHandler> m_event_handler;
    std::shared_ptr<VulkanRenderer> m_renderer;
};

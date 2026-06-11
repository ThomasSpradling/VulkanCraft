#pragma once

#include "../Application/InputHandler.h"
class Application;
#include "../Graphics/VulkanRenderer.h"

class IGame {
public:
    IGame() {}

    void SetUp(Application *application, std::shared_ptr<InputHandler> input_handler, std::shared_ptr<VulkanRenderer> renderer) {
        m_application = application;
        m_input_handler = input_handler;
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
    std::shared_ptr<InputHandler> m_input_handler;
    std::shared_ptr<VulkanRenderer> m_renderer;
};

#pragma once

#include <iostream>
#include <memory>
#include <optional>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "../Events/EventHandler.h"
#include "../Events/WindowEvents/KeyEvents.h"
#include "IGame.h"
#include "../Graphics/VulkanRenderer.h"


/**
 * @brief Manages the program as a software-level application. This includes
 * window management, initialization of core libraries, swapchain loop logic,
 * and inputs.
 * 
 */
class Application {
public:
    Application(std::shared_ptr<IGame> game);
    ~Application();

    void Run();
    glm::ivec2 GetWindowSize();
private:
    struct Mouse {
        bool grabbed = false;
        glm::vec2 position = glm::vec2(0.0f);
    };
private:
    std::shared_ptr<EventHandler> m_event_handler = nullptr;
    std::shared_ptr<InputHandler> m_input_handler = nullptr;
    std::shared_ptr<IGame> m_game = nullptr;

    GLFWwindow *m_window;

    std::shared_ptr<VulkanRenderer> m_vulkan_renderer;

    static constexpr int UPDATE_RATE = 120; // Hz
    std::optional<uint32_t> m_target_fps = std::nullopt; // If nullopt, then uncapped FPS
    uint32_t m_fps = 0;

    Mouse m_mouse {};
private:
    void Render(float delta_time);
    void Update(float delta_time);

    void static GlfwErrorCallback(int code, const char *description);

    void static KeyCallback(GLFWwindow *window, int key, int, int action, int);
    void static MouseButtonCallback(GLFWwindow *window, int button, int action, int);
    void static MouseMoveCallback(GLFWwindow *window, double x, double y);
    void static ScrollCallback(GLFWwindow *window, double, double y_offset);
};
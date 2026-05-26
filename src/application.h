#pragma once

#include "GLFW/glfw3.h"
#include "vulkan_renderer.h"
#include "utils/vulkan.h"

/**
 * @brief Manages the program as a software-level application. This includes
 * window management, initialization of core libraries, swapchain loop logic,
 * and inputs.
 *
 * @note Should not include any application specific code or graphics rendering!
 * 
 */
class Application {
public:
    Application();
    ~Application();

    void Run();
private:
    GLFWwindow *m_window;
    VulkanRenderer m_vulkan_renderer;

    bool m_fatal_error = false;
};
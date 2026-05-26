#include "application.h"
#include <iostream>

Application::Application() {
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!\n";
        m_fatal_error = true;
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    const int width = 1080;
    const int height = 720;
    const char *title = "Tiny Minecraft";
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Could not create window!\n";
        m_fatal_error = true;
        return;
    }

    m_vulkan_renderer.Initialize(m_window);
}

Application::~Application() {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Application::Run() {
    if (m_fatal_error) {
        exit(1);
    }

    std::cout << "Hello, world!\n";
}
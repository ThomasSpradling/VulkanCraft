#include "Window.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <string>

Window::Window(const WindowConfig &config) {
    glfwSetErrorCallback(Window::ErrorCallback);
    if (!glfwInit()) {
        throw std::runtime_error("Could not initialize GLFW!\n");
    }
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(config.resolution.x, config.resolution.y, config.title.c_str(), nullptr, nullptr);
    if (m_window == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Could not create GLFW window!\n");
    }
    
    if (config.fullscreen) {
        MakeFullscreen();
    } else {
        MakeWindowed();
    }
    glfwGetFramebufferSize(m_window, &m_resolution.x, &m_resolution.y);
    m_window_resized = true;

    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, KeyCallback);
    glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
    glfwSetCursorPosCallback(m_window, MouseMoveCallback);
    glfwSetScrollCallback(m_window, ScrollCallback);
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);
    glfwSetWindowIconifyCallback(m_window, IconifyCallback);
}

Window::~Window() {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::SetTitle(std::string_view title) {
    std::string title_str(title);
    glfwSetWindowTitle(m_window, title_str.c_str());
}

void Window::DisableCursor() const {
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Window::EnableCursor() const {
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void Window::MakeFullscreen() {
    GLFWmonitor *current_monitor = glfwGetWindowMonitor(m_window);
    if (current_monitor == nullptr) {
        current_monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(current_monitor);
        
        glfwGetWindowPos(m_window, &m_windowed_position.x, &m_windowed_position.y);
        glfwGetWindowSize(m_window, &m_windowed_size.x, &m_windowed_size.y);
        glfwSetWindowMonitor(m_window, current_monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    
        glfwGetFramebufferSize(m_window, &m_resolution.x, &m_resolution.y);
        m_window_resized = true;
    }
}

void Window::MakeWindowed() {
    GLFWmonitor *current_monitor = glfwGetWindowMonitor(m_window);
    if (current_monitor != nullptr) {
        glfwSetWindowMonitor(
            m_window, nullptr,
            m_windowed_position.x, m_windowed_position.y,
            m_windowed_size.x, m_windowed_size.y,
            GLFW_DONT_CARE
        );

        glfwGetFramebufferSize(m_window, &m_resolution.x, &m_resolution.y);
        m_window_resized = true;
    }
}

bool Window::WasResized() {
    if (m_window_iconified || m_resolution == glm::ivec2(0))
        return false;

    return std::exchange(m_window_resized, false);
}

void Window::ErrorCallback(int code, const char *description) {
    std::cerr << "GLFW Error " << code << ": " << description << "\n"; 
}

void Window::KeyCallback(GLFWwindow *handle, int key, int, int action, int) {
    auto *window = static_cast<Window *>(glfwGetWindowUserPointer(handle));
    if (window == nullptr) return;
    if (key < 0 || key > GLFW_KEY_LAST) return;
    
    switch (action) {
        case GLFW_PRESS:
        case GLFW_REPEAT:
            window->m_keyboard.keys[key].down = true;
            window->m_keyboard.keys[key].pressed = true;
            break;
        case GLFW_RELEASE:
            window->m_keyboard.keys[key].down = false;
            break;
        default:
            break;
    }
}

void Window::MouseButtonCallback(GLFWwindow *handle, int button, int action, int) {
    auto *window = static_cast<Window *>(glfwGetWindowUserPointer(handle));
    if (window == nullptr) return;
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return;

    switch (action) {
        case GLFW_PRESS:
        case GLFW_REPEAT:
            window->m_mouse.buttons[button].down = true;
            window->m_mouse.buttons[button].pressed = true;
            break;
        case GLFW_RELEASE:
            window->m_mouse.buttons[button].down = false;
            break;
        default:
            break;
    }
}

void Window::MouseMoveCallback(GLFWwindow *handle, double x, double y) {
    auto *window = static_cast<Window *>(glfwGetWindowUserPointer(handle));
    if (window == nullptr) return;

    if (!window->m_mouse.grabbed) {
        window->m_mouse.position = { x, y };
        window->m_mouse.grabbed = true;
        return;
    }

    glm::vec2 current_position { x, y };
    glm::vec2 offset = current_position - window->m_mouse.position;
    window->m_mouse.position = current_position;
    window->m_mouse.delta = offset;
}

void Window::ScrollCallback(GLFWwindow *handle, double, double offset_y) {
    auto *window = static_cast<Window *>(glfwGetWindowUserPointer(handle));
    if (window == nullptr) return;

    window->m_mouse.wheel.offset_y = static_cast<float>(offset_y);
}

void Window::FramebufferResizeCallback(GLFWwindow *handle, int width, int height) {
    auto *window = static_cast<Window *>(glfwGetWindowUserPointer(handle));
    if (window == nullptr) return;

    if (window->m_window_iconified || glm::ivec2(width, height) == glm::ivec2(0))
        return;

    window->m_resolution = glm::ivec2(width, height);
    window->m_window_resized = true;
}

void Window::IconifyCallback(GLFWwindow *handle, int iconified) {
    auto *window = static_cast<Window *>(glfwGetWindowUserPointer(handle));
    if (window == nullptr) return;

    window->m_window_iconified = iconified == GLFW_TRUE;
}

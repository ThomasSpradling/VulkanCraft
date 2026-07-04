#include "Window.h"
#include <iostream>
#include <stdexcept>
#include <string>

Window::Window(int width, int height, std::string_view title) {
    if (!glfwInit()) {
        throw std::runtime_error("Could not initialize GLFW!\n");
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwSetErrorCallback(Window::ErrorCallback);

    std::string title_str(title);
    m_handle = glfwCreateWindow(width, height, title_str.c_str(), nullptr, nullptr);
    if (!m_handle) {
        glfwTerminate();
        throw std::runtime_error("Could not create window!");
        return;
    }

    glfwSetWindowUserPointer(m_handle, this);

    glfwSetKeyCallback(m_handle, KeyCallback);
    glfwSetMouseButtonCallback(m_handle, MouseButtonCallback);
    glfwSetCursorPosCallback(m_handle, MouseMoveCallback);
    glfwSetScrollCallback(m_handle, ScrollCallback);
}

Window::~Window() {
    glfwDestroyWindow(m_handle);
    glfwTerminate();
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_handle);
}

void Window::SetTitle(std::string_view title) {
    std::string title_str(title);
    glfwSetWindowTitle(m_handle, title_str.c_str());
}

void Window::DisableCursor() const {
    glfwSetInputMode(m_handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Window::EnableCursor() const {
    glfwSetInputMode(m_handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

glm::ivec2 Window::GetSize() {
    glm::ivec2 result;
    glfwGetWindowSize(m_handle, &result.x, &result.y);
    return result;
}

void Window::ErrorCallback(int code, const char *description) {
    std::cerr << "GLFW Error " << code << ": " << description << "\n"; 
}


void Window::KeyCallback(GLFWwindow *handle, int key, int, int action, int) {
    auto *window = static_cast<Window *>(glfwGetWindowUserPointer(handle));
    if (window == nullptr) return;
    
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

    switch (action) {
        case GLFW_PRESS:
        case GLFW_REPEAT:
            window->m_mouse.buttons[button].down = true;
            window->m_keyboard.keys[button].pressed = true;
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

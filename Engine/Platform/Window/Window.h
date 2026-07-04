#pragma once

#include <GLFW/glfw3.h>
#include <memory>
#include <string>
#include <string_view>

#include "Core/NonMovable.h"
#include "Core/NonCopyable.h"
#include "InputHandler.h"

#include <glm/glm.hpp>

struct WindowConfig {
    glm::ivec2 resolution = glm::ivec2(1280, 720);
    std::string title = "Simple Window";
    bool fullscreen = false;
};

class Window : public NonMovable, public NonCopyable {
    friend InputHandler;
public:
    Window(const WindowConfig &config);
    ~Window();

    bool ShouldClose() const;
    void SetTitle(std::string_view title);
    void DisableCursor() const;
    void EnableCursor() const;

    void MakeFullscreen();
    void MakeWindowed();
    bool WasResized();
    inline bool IsIconified() const { return m_window_iconified; };

    GLFWwindow *GetHandle() const { return m_window; }
    glm::ivec2 GetFramebufferSize() const { return m_resolution; }
private:
    struct Button {
        bool pressed = false;
        bool down = false;
    };

    struct Keyboard {
        std::array<Button, GLFW_KEY_LAST> keys;
    } m_keyboard;

    struct Mouse {
        std::array<Button, GLFW_MOUSE_BUTTON_LAST> buttons;
        glm::vec2 delta { 0.0f };
        glm::vec2 position { 0.0f };
        bool grabbed = false;

        struct Wheel {
            float offset_y = 0.0f;
        } wheel;
    } m_mouse;
private:
    bool m_window_resized = false;
    bool m_window_iconified = false;

    // Framebuffer size
    glm::ivec2 m_resolution {};

    // Cached window positions/size. May be out-of-date until window monitor state changes.
    glm::ivec2 m_windowed_position {};
    glm::ivec2 m_windowed_size {};

    GLFWmonitor *m_monitor = nullptr;
    GLFWwindow *m_window = nullptr;
private:
    void static ErrorCallback(int code, const char *description);
    void static KeyCallback(GLFWwindow *handle, int key, int, int action, int);
    void static MouseButtonCallback(GLFWwindow *handle, int button, int action, int);
    void static MouseMoveCallback(GLFWwindow *handle, double x, double y);
    void static ScrollCallback(GLFWwindow *handle, double, double offset_y);

    void static FramebufferResizeCallback(GLFWwindow *handle, int width, int height);
    void static IconifyCallback(GLFWwindow *handle, int iconified);
};

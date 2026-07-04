#pragma once

#include <GLFW/glfw3.h>
#include <memory>
#include <string_view>

#include "Core/NonMovable.h"
#include "Core/NonCopyable.h"
#include "InputHandler.h"

#include <glm/glm.hpp>

class Window : public NonMovable, public NonCopyable {
    friend InputHandler;
public:
    Window(int width, int height, std::string_view title);
    ~Window();

    bool ShouldClose() const;
    void SetTitle(std::string_view title);
    void DisableCursor() const;
    void EnableCursor() const;

    GLFWwindow *GetHandle() const { return m_handle; }

    glm::ivec2 GetSize();
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

    GLFWwindow *m_handle = nullptr;
private:
    void static ErrorCallback(int code, const char *description);
    void static KeyCallback(GLFWwindow *window, int key, int, int action, int);
    void static MouseButtonCallback(GLFWwindow *window, int button, int action, int);
    void static MouseMoveCallback(GLFWwindow *window, double x, double y);
    void static ScrollCallback(GLFWwindow *window, double, double offset_y);
    void static ResizeCallback(GLFWwindow *window, int width, int height);
};

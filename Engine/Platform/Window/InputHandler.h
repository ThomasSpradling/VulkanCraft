#pragma once

#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include <array>
#include <glm/glm.hpp>

class Window;
class InputHandler : public NonMovable, public NonCopyable {
public:
    InputHandler(Window &window);
    ~InputHandler() = default;

    // Note: Should be called as late as possible in the update cycle since it handles
    // pressing logic
    void Update();

    bool IsKeyPressed(int key) const;
    bool IsKeyDown(int key) const;
    bool IsKeyUp(int key) const;

    bool IsMouseButtonPressed(int button) const;
    bool IsMouseButtonDown(int button) const;
    bool IsMouseButtonUp(int button) const;

    glm::vec2 MousePosition() const;
    glm::vec2 MouseOffset() const;

    float ScrollOffset() const;
private:
    Window &m_window;
};

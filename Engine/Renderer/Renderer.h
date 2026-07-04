#pragma once

#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include "Platform/Window/Window.h"

class Renderer : public NonCopyable, public NonMovable {
public:
    Renderer(const Window &window);
    ~Renderer();
private:
};

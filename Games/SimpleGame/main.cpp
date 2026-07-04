#include <iostream>
#include "Example.h"
// #include "Platform/Window/Window.h"

#include <Engine/Client.h>
#include <thread>
#include <chrono>

int main(int argc, char *argv[]) {
    using namespace std::chrono_literals;

    std::cout << "Hello, world: " << f() << "\n";

    Window window = Window(WindowConfig {
        .resolution = glm::vec2(800, 400),
        .title = "Hello",
        .fullscreen = false,
    });

    while (!window.ShouldClose()) {
        glfwPollEvents();
        
        std::cout << "X: " << window.GetFramebufferSize().x;
        std::cout << ", Y: " << window.GetFramebufferSize().y;
        std::cout << ", WAS_RESIZED: " << (window.WasResized() ? "TRUE" : "FALSE");
        std::cout << ", ICON: " << (window.IsIconified() ? "TRUE" : "FALSE") << "\n";

        std::this_thread::sleep_for(100ms);
    }
}

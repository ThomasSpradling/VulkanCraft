#include <cstdio>
#include <iostream>
#include "application.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

int main(int argc, char *argv[]) {
    try {
        Application app{};
        app.Run();
    } catch (const std::runtime_error &error) {
        std::cerr << "\033[31mRuntime Error: " << error.what() << "\033[0m\n";
        exit(1);
    }
    exit(0);
}
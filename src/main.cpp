#include <cstdio>
#include <iostream>
#include "application.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <stdexcept>

#include "pong_game/pong_game.h"

int main(int argc, char *argv[]) {
    try {
        // ConsoleGame game;
        std::shared_ptr<Game> game = std::make_shared<PongGame>();
        Application app{game};
        app.Run();
    } catch (const std::runtime_error &error) {
        std::cerr << "\033[31mRuntime Error: " << error.what() << "\033[0m\n";
        exit(1);
    }
    exit(0);
}
#include <iostream>
#include "Application/Application.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <stdexcept>

#include "Client/GameClient.h"
#include "Server/GameServer.h"

int main(int argc, char *argv[]) {
    if (argc >= 2 && std::strcmp(argv[1], "--server") == 0) {
        try {
            GameServer game_server{};
            game_server.Run();
            
            return 0;
        } catch (const std::runtime_error &error) {
            std::cerr << "\033[31mRuntime Error: " << error.what() << "\033[0m\n";
            return 1;
        }
    }

    try {
        std::shared_ptr<IGame> game = std::make_shared<GameClient>();
        Application app{game};
        app.Run();
        return 0;
    } catch (const std::runtime_error &error) {
        std::cerr << "\033[31mRuntime Error: " << error.what() << "\033[0m\n";
        return 1;
    }
    return 0;
}

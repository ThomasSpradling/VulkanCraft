#include <iostream>
#include "SimpleGame.h"
// #include "Platform/Window/Window.h"

#include <Engine/Client.h>
#include <thread>
#include <chrono>

int main(int argc, char *argv[]) {
    try {
        SimpleGame game{};
        ClientApplication client_application(game, ClientEngineConfig{
            .window_width = 1080,
            .window_height = 720,
            .window_title = "Simple Game",
            .update_rate = 120,
        });
        
        client_application.Run();

        return 0;
    } catch (const std::runtime_error &error) {
        std::cerr << "\033[31mRuntime Error: " << error.what() << "\033[0m\n";
        return 1;
    } catch (const std::exception &error) {
        std::cerr << "\033[31mUnhandled Exception: " << error.what() << "\033[0m\n";
        return 1;
    } catch (...) {
        std::cerr << "\033[31mUnknown fatal exception\033[0m\n";
        return 1;
    }
}

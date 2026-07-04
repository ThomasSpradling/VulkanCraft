#include "Platform/Sockets/ISocket.h"
#include <iostream>

#include <Engine/Server.h>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    try {
        SocketAPI socket_api {};
        socket_api.Initialize();

        NetworkHost host(HostType::Server, NetworkAddress::Any(8888));
        while(true) {
            host.Update(100);

            NetworkCommand command;
            while (host.PollNetworkCommand(command)) {
                if (command.type == NetworkCommandType::Connect) {
                    std::cout << "Connection!\n";
                }
            }

            std::this_thread::sleep_for(100ms);
        }
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

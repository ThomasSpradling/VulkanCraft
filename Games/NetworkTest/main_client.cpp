#include "Network/NetworkHost.h"
#include <iostream>

#include <Engine/Client.h>
#include <thread>
#include <chrono>

int main() {
    using namespace std::chrono_literals;

    try {
        SocketAPI api {};
        api.Initialize();

        NetworkHost host(HostType::Client);
        host.Connect(NetworkAddress::Localhost(8888));

        while(true) {
            host.Update(80);

            NetworkCommand command;
            while (host.PollNetworkCommand(command)) {
                if (command.type == NetworkCommandType::Connect) {
                    std::cout << "Connection!\n";
                }
            }

            std::this_thread::sleep_for(80ms);
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

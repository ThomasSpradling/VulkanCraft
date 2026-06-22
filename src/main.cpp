#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include "Application/Application.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Client/GameClient.h"
#include "Network/NetworkHost.h"
#include "Network/Packets.h"
#include "Network/Packets/ClientPackets.h"
#include "Platform/Sockets/SocketAPI.h"
#include "Server/GameServer.h"

#include "Platform/Sockets/Socket.h"

// void SendLargeData() {

// }

// Reliable Large
// Reliable Messages
// Unreliable Fragmented

Packet GenerateLargePacket() {
    std::vector<uint32_t> data(100);
    for (uint32_t i = 0; i < 100; ++i) {
        data[i] = i;
    }

    Packet packet {
        .packet_type = PacketType::LargeTest,
        .packet_data = PacketLargeTest{
            .num_items = 100,
            .items = data,
        },
    };
    return packet;
}

// void PrintLargePacket() {

// }

// struct Entry {
//     std::unordered_map<uint32_t, PacketFragment> data;
// };

// struct SequenceBuffer {
//     std::array<uint32_t, 256> sequence;
//     std::array<Entry, 256> entries;
// };

// void SendPacket(Socket &socket, Packet packet, NetworkAddress &to) {
//     NetworkBuffer buffer{};
//     std::vector<Packet> packets = SplitPacket(packet);
//     for (auto &packet : packets) {
//         buffer.Clear();
//         packet.Write(buffer);
//         socket.SendTo(buffer, to);
//     }
// }

void ReceivePacket() {

}

int main(int argc, char *argv[]) {
    // if (argc >= 2 && std::strcmp(argv[1], "--server") == 0) {
    //     std::cout << "Server starting!\n";
    //     GameServer game_server{};
    //     game_server.Run();
    //     return 0;
    // }

    // std::shared_ptr<IGame> game = std::make_shared<GameClient>();
    // Application app{game};
    // try {
    //     app.Run();
    // } catch (const std::runtime_error &error) {
    //     std::cerr << "\033[31mRuntime Error: " << error.what() << "\033[0m\n";
    //     exit(1);
    // }
    // exit(0);

    // Temporary:

    using namespace std::chrono_literals;
    if (argc >= 2 && std::strcmp(argv[1], "--server") == 0) {
        try {
            SocketAPI api{};
            api.Initialize();
    
            NetworkAddress address = NetworkAddress::Any(PROTOCOL_PORT);
            NetworkHost server(HostType::Server, address, 2);

            std::cout << "Server running at address " << address.AddressName() << " on port " << address.Port() << "\n";

            while(true) {
                // receive packets
                NetworkPeer peer;
                while (auto packet = server.ReceivePacket(peer)) {
                    switch (packet->packet_type) {
                        case PacketType::Test: {
                            PacketTest test = std::get<PacketTest>(packet->packet_data);
                            std::cout << "Received test packet: " << test.value << std::endl;
                            break;
                        }
                        default:
                            break;
                    }
                }

                std::this_thread::sleep_for(100ms);
            }

            // std::cout << "The lucky number is: " << pack.value << std::endl;
        } catch (const std::runtime_error &error) {
            std::cerr << "\033[31mRuntime Error: " << error.what() << "\033[0m\n";
            return 1;
        }

        return 0;
    }

    try {
        
        SocketAPI api{};
        api.Initialize();
        
        NetworkHost client(HostType::Client);
        NetworkAddress server_address {"127.0.0.1", PROTOCOL_PORT};

        std::cout << "Client started up!\n";
        
        NetworkPeer server {
            .address = server_address,
        };

        client.ConnectToHost(server_address);

        Packet packet {
            .packet_type = PacketType::Test,
            .sequence = 0,
            .packet_data = PacketTest{
                .value = 10001,
            }
        };
        client.SendPacket(server, packet);

        while(true) {
            // receive packets
            NetworkPeer peer;
            while (auto packet = client.ReceivePacket(peer)) {
                switch (packet->packet_type) {
                    case PacketType::Test: {
                        PacketTest test = std::get<PacketTest>(packet->packet_data);
                        std::cout << "Received test packet: " << test.value << std::endl;
                        break;
                    }
                    default:
                        break;
                }
            }

            std::this_thread::sleep_for(100ms);
        }

        client.DisconnectFromHost(server_address);


        std::cout << "We sent some data to " << server_address.AddressName() << " at port " << server_address.Port() << " \n";
        return 0;
    } catch (const std::runtime_error &error) {
        std::cerr << "\033[31mRuntime Error: " << error.what() << "\033[0m\n";
        return 1;
    }

    return 0;
}

// NetworkAddress address = NetworkAddress::Any(8888);
            // NetworkHost server(address);

            // std::cout << "Server running at address " << address.AddressName() << " on port " << address.Port() << "\n";

            // while(true) {
            //     // receive packets
            //     NetworkPeer peer;
            //     while (auto packet = server.ReceivePacket(peer)) {
            //         switch (packet->packet_type) {
            //             case PacketType::Test: {
            //                 PacketTest test = std::get<PacketTest>(packet->packet_data);
            //                 std::cout << "Received test packet: " << test.value << std::endl;
            //                 break;
            //             }
            //             default:
            //                 break;
            //         }
            //     }

            //     std::this_thread::sleep_for(100ms);
            // }
    
            // SocketAPI api{};

// NetworkHost client {};

        // std::cout << "Client started up!\n";
        
        // NetworkPeer server {
        //     .address = server_address,
        // };
        // Packet packet {
        //     .packet_type = PacketType::Test,
        //     .packet_data = PacketTest{
        //         .value = 10001,
        //     }
        // };
        // client.SendPacket(server, packet);

        // while(true) {
        //     // receive packets
        //     NetworkPeer peer;
        //     while (auto packet = client.ReceivePacket(peer)) {
        //         switch (packet->packet_type) {
        //             case PacketType::Test: {
        //                 PacketTest test = std::get<PacketTest>(packet->packet_data);
        //                 std::cout << "Received test packet: " << test.value << std::endl;
        //                 break;
        //             }
        //             default:
        //                 break;
        //         }
        //     }

        //     std::this_thread::sleep_for(100ms);
        // }
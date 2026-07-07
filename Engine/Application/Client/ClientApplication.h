#pragma once

#include "IClientGame.h"
#include "Network/NetworkHost.h"
#include "Platform/Sockets/SocketAPI.h"
#include <memory>
#include <optional>

struct ClientEngineConfig {
    uint32_t window_width = 720;
    uint32_t window_height = 480;

    std::string window_title = "Vulkan Engine";

    uint32_t update_rate = 120;
    std::optional<uint32_t> target_fps = std::nullopt; // If nullopt, then uncapped FPS
};

/**
 * @brief Manages the program as a software-level application. This includes
 * window management, initialization of core libraries, runtime loop logic,
 * and inputs.
 */
class ClientApplication {
public:
    explicit ClientApplication(IClientGame &game, const ClientEngineConfig &config);
    ~ClientApplication();

    void Run();

    void SetTargetFPS(uint32_t fps) { m_target_fps = fps; }
    void UncapFPS() { m_target_fps = std::nullopt; }
private:
    IClientGame &m_game;

    bool m_running = false;

    std::unique_ptr<Window> m_window = nullptr;
    std::unique_ptr<InputHandler> m_input_handler = nullptr;
    std::unique_ptr<Renderer> m_renderer = nullptr;

    std::unique_ptr<SocketAPI> m_socket_api = nullptr;
    std::unique_ptr<NetworkHost> m_network_host = nullptr;
    
    uint32_t m_update_rate = 120; // Hz
    std::optional<uint32_t> m_target_fps = std::nullopt; // If nullopt, then uncapped FPS

    const double FpsRecordTime = 500.0; // ms
    uint32_t m_fps = 0;
};
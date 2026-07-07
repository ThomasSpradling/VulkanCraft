#include "ClientApplication.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "Network/NetworkHost.h"
#include "Platform/Sockets/SocketAPI.h"

ClientApplication::ClientApplication(IClientGame &game, const ClientEngineConfig &config)
    : m_game(game)
{
    m_window = std::make_unique<Window>(WindowConfig{
        .resolution = glm::vec2(config.window_width, config.window_height),
        .title = config.window_title,
    });

    m_renderer = std::make_unique<Renderer>(*m_window);
    m_input_handler = std::make_unique<InputHandler>(*m_window);

    m_socket_api = std::make_unique<SocketAPI>();
    m_socket_api->Initialize();

    m_network_host = std::make_unique<NetworkHost>(HostType::Client);
}

ClientApplication::~ClientApplication() {
    m_network_host.reset();
    m_socket_api.reset();

    m_input_handler.reset();
    m_renderer.reset();

    m_window.reset();

    std::cout << "Destroyed ClientApplication!\n";
}

void ClientApplication::Run() {
    using clock = std::chrono::steady_clock;

    auto previous_time = clock::now();

    double update_accum = 0.0;
    double render_accum = 0.0;

    uint32_t frame_count = 0;
    double fps_timer = 0.0;

    const double UPDATE_STEP_TIME = 1000.0 / m_update_rate;
    constexpr double MAX_FRAME_WAIT_TIME_MS = 250.0;

    ClientContext context {
        .input_handler = *m_input_handler,
        .renderer = *m_renderer,
        .client_host = *m_network_host,
    };

    m_running = true;

    m_game.Initialize(context);
    while (m_running) {
        if (m_window->ShouldClose()) {
            m_running = false;
        }

        const bool HAS_CAPPED_FPS = m_target_fps.has_value();
        glfwPollEvents();

        auto current_time = clock::now();
        std::chrono::duration<double, std::milli> delta_time = current_time - previous_time;
        previous_time = current_time;
        
        update_accum += delta_time.count();

        double frame_time = std::clamp(delta_time.count(), 0.0, MAX_FRAME_WAIT_TIME_MS);
        render_accum += frame_time;
        fps_timer += frame_time;

        while (update_accum >= UPDATE_STEP_TIME) {
            m_game.Update(UPDATE_STEP_TIME, context);
            m_input_handler->Update();
            update_accum -= UPDATE_STEP_TIME;
        }

        if (!HAS_CAPPED_FPS) {
            m_game.Render(frame_time, context);
            frame_count++;
        } else if (const double render_step_time = 1000.0 / m_target_fps.value(); render_accum >= render_step_time) {
            m_game.Render(render_step_time, context);
            frame_count++;
            render_accum -= render_step_time;

            double time_until_next_update = UPDATE_STEP_TIME - update_accum;
            double time_until_next_render = render_step_time - render_accum;
            double sleep_time = std::min<double>(time_until_next_update, time_until_next_render);

            if (sleep_time > 0.0) {
                std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(sleep_time));
            }
        }

        if (fps_timer >= FpsRecordTime) {
            m_fps = static_cast<uint32_t>(static_cast<double>(frame_count) * 1000.0 / fps_timer);
            std::cout << "FPS: " << m_fps << "\n";
            frame_count = 0;
            fps_timer = 0.0;
        }
    }
    m_game.ShutDown(context);
}

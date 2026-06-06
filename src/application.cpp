#include "Events/WindowEvents/mouse_events.h"
#include "Graphics/utils.h"

#include "application.h"
#include "vulkan/vulkan_core.h"
#include "Graphics/vulkan_renderer.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include "errors.h"

Application::Application(std::shared_ptr<Game> game)
    : m_game(game)
{
    if (!glfwInit()) {
        throw std::runtime_error("Could not initialize GLFW!\n");
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwSetErrorCallback(GlfwErrorCallback);

    const int width = 1080;
    const int height = 720;
    const char *title = "Tiny Minecraft";
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Could not create window!");
        return;
    }

    glfwSetWindowUserPointer(m_window, this);

    // Add callbacks
    glfwSetKeyCallback(m_window, KeyCallback);
    glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
    glfwSetCursorPosCallback(m_window, MouseMoveCallback);
    glfwSetScrollCallback(m_window, ScrollCallback);

    m_vulkan_renderer = std::make_shared<VulkanRenderer>(*m_window);

    m_vulkan_renderer->Properties().window_extent = { width, height };
    m_vulkan_renderer->Initialize();

    m_event_handler = std::make_shared<EventHandler>();

    m_game->SetUp(this, m_event_handler, m_vulkan_renderer);
}

Application::~Application() {
    vkDeviceWaitIdle(m_vulkan_renderer->GetDevice());

    m_game->ShutDown();
    m_vulkan_renderer->ShutDown();

    glfwDestroyWindow(m_window);
    glfwTerminate();

    std::cout << "Destroyed application!\n";
}

void Application::Run() {
    using clock = std::chrono::high_resolution_clock;

    auto previous_time = clock::now();

    double update_accum = 0.0;
    double render_accum = 0.0;

    uint32_t frame_count = 0;
    double fps_timer = 0.0;

    const double UPDATE_STEP_TIME = 1000.0 / UPDATE_RATE;
    constexpr double MAX_FRAME_WAIT_TIME_MS = 250.0;

    m_game->Initialize();
    while (!glfwWindowShouldClose(m_window)) {
        const bool HAS_CAPPED_FPS = m_target_fps.has_value();
        glfwPollEvents();

        auto current_time = clock::now();
        std::chrono::duration<double, std::milli> delta_time = current_time - previous_time;
        previous_time = current_time;
        
        update_accum += delta_time.count();

        double frame_time = std::clamp(delta_time.count(), 0.0, MAX_FRAME_WAIT_TIME_MS);
        render_accum += frame_time;
        fps_timer += frame_time;
        frame_count++;

        while (update_accum >= UPDATE_STEP_TIME) {
            Update(UPDATE_STEP_TIME);
            update_accum -= UPDATE_STEP_TIME;
        }

        if (!HAS_CAPPED_FPS) {
            Render(frame_time);
        } else if (const double render_step_time = 1000.0 / m_target_fps.value(); render_accum >= render_step_time) {
            Render(render_step_time);
            render_accum -= render_step_time;

            double time_until_next_update = UPDATE_STEP_TIME - update_accum;
            double time_until_next_render = render_step_time - render_accum;
            double sleep_time = std::min<double>(time_until_next_update, time_until_next_render);

            if (sleep_time > 0.0) {
                std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(sleep_time));
            }
        }

        if (fps_timer >= 1000.0) {
            m_fps = frame_count;
            std::cout << "FPS: " << m_fps << std::endl;
            frame_count = 0;
            fps_timer -= 1000.0;
        }
    }
}

glm::ivec2 Application::GetWindowSize() {
    int width, height;
    glfwGetWindowSize(m_window, &width, &height);
    return { width, height };
}

void Application::Render(float delta_time) {
    auto frame = m_vulkan_renderer->BeginFrame();
    m_game->Render(frame, delta_time);

    if (frame)
        m_vulkan_renderer->EndFrame();
}

void Application::Update(float delta_time) {
    m_game->Update(delta_time);
}

void Application::GlfwErrorCallback(int code, const char *description) {
    std::cerr << "GLFW Error " << code << ": " << description << "\n"; 
}

void Application::KeyCallback(GLFWwindow *window, int key, int, int action, int) {
    auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
    if (app == nullptr) return;

    switch (action) {
        case GLFW_PRESS: {
            KeyPressedEvent event(key, false);
            app->m_event_handler->SignalEvent(event);
            break;
        }
        case GLFW_REPEAT: {
            KeyPressedEvent event(key, true);
            app->m_event_handler->SignalEvent(event);
            break;
        }
        case GLFW_RELEASE: {
            KeyReleasedEvent event(key);
            app->m_event_handler->SignalEvent(event);
            break;
        }
    }
}

void Application::MouseButtonCallback(GLFWwindow *window, int button, int action, int) {
    auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
    if (app == nullptr) return;

    switch (action) {
        case GLFW_PRESS: {
            MouseButtonPressedEvent event(button);
            app->m_event_handler->SignalEvent(event);
            break;
        }
        case GLFW_RELEASE: {
            MouseButtonReleasedEvent event(button);
            app->m_event_handler->SignalEvent(event);
            break;
        }
    }
}

void Application::MouseMoveCallback(GLFWwindow *window, double x, double y) {
    auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
    if (app == nullptr) return;

    if (!app->m_mouse.grabbed) {
        app->m_mouse.position = { x, y };
        app->m_mouse.grabbed = true;
    }

    glm::vec2 current_position { x, y };
    glm::vec2 offset = current_position - app->m_mouse.position;
    app->m_mouse.position = current_position;

    MouseMovedEvent event(current_position, offset);
    app->m_event_handler->SignalEvent(event);    
}

void Application::ScrollCallback(GLFWwindow *window, double, double y_offset) {
    auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
    if (app == nullptr) return;

    MouseScrolledEvent event(y_offset);
    app->m_event_handler->SignalEvent(event);
}

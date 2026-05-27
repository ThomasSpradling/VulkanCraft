#include "../utils/vulkan.h"

#include "../events/window_events/key_events.h"
#include "../events/window_events/mouse_events.h"
#include "../events/event_dispatcher.h"
#include "pong_game.h"
#include <iostream>

void PongGame::Initialize() {
    std::cout << "Game initialized!" << std::endl;
    m_event_handler->AddListener(this);
}

void PongGame::ShutDown() {
    std::cout << "Game shutting down!" << std::endl;
    m_event_handler->RemoveListener(this);
}

void PongGame::Update(float delta_time) {

}

void PongGame::Render(const Frame &frame, float delta_time) {
    // Transition to VK_IMAGE_LAYOUT_GENERAL
    {
        VkImageMemoryBarrier2 draw_image_barrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = frame.image,
            .subresourceRange = IMAGE_SUBRESOURCE_RANGE_DEFAULT,
        };
        
        VkDependencyInfo dependency_info {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &draw_image_barrier,
        };

        vkCmdPipelineBarrier2(frame.cmd, &dependency_info);
    }

    // VkClearColorValue clear_color = { 164.0f/256.0f, 30.0f/256.0f, 34.0f/256.0f, 0.0f };
    vkCmdClearColorImage(frame.cmd, frame.image, VK_IMAGE_LAYOUT_GENERAL, &m_clear_color, 1, &IMAGE_SUBRESOURCE_RANGE_DEFAULT);
}

void PongGame::OnEvent(const Event &event) {
    EventDispatcher dispatcher(event);

    dispatcher.Dispatch<MouseMovedEvent>([this](const MouseMovedEvent &e) {
        glm::vec2 screen_size = m_application->GetWindowSize();
        glm::vec2 color = (e.GetMousePosition() / screen_size);
        m_clear_color = { color.r, color.g, 0.0f, 0.0f };
    });
}

#include "utils/vulkan.h"

#include "application.h"
#include "vulkan/vulkan_core.h"
#include "vulkan_renderer.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include "utils/errors.h"

Application::Application() {
    if (!glfwInit()) {
        throw std::runtime_error("Could not initialize GLFW!\n");
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    const int width = 1080;
    const int height = 720;
    const char *title = "Tiny Minecraft";
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Could not create window!");
        return;
    }

    m_vulkan_renderer = std::make_unique<VulkanRenderer>(*m_window);

    m_vulkan_renderer->Properties().window_extent = { width, height };
    m_vulkan_renderer->Initialize();
}

Application::~Application() {
    m_vulkan_renderer->ShutDown();

    glfwDestroyWindow(m_window);
    glfwTerminate();

    std::cout << "Destroyed application!\n";
}

void Application::Run() {
    std::cout << "Hello, world111!\n";

    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // TODO: Cap FPS
        // Update
        // Render

        if (auto frame = m_vulkan_renderer->BeginFrame(); frame) {
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
                    .image = frame->image,
                    .subresourceRange = IMAGE_SUBRESOURCE_RANGE_DEFAULT,
                };
                
                VkDependencyInfo dependency_info {
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .pNext = nullptr,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = &draw_image_barrier,
                };

                vkCmdPipelineBarrier2(frame->cmd, &dependency_info);
            }

            VkClearColorValue clear_color = { 164.0f/256.0f, 30.0f/256.0f, 34.0f/256.0f, 0.0f };
            vkCmdClearColorImage(frame->cmd, frame->image, VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &IMAGE_SUBRESOURCE_RANGE_DEFAULT);
            m_vulkan_renderer->EndFrame();
        }
    }
}
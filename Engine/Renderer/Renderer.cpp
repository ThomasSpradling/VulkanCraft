#include "Renderer.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Platform/Graphics/Common.h"
#include "Platform/Graphics/PipelineBuilder.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanDevice.h"
#include "Platform/Graphics/VulkanImage.h"
#include <filesystem>
#include <format>
#include <iostream>
#include <vector>

Renderer::Renderer(const Window &window)
    : m_window(window)
{
    std::cout << "Initializing Renderer\n";
    m_device = std::make_unique<VulkanDevice>(window);

    m_swapchain = std::make_unique<VulkanSwapChain>(*m_device, SwapChainConfig{
        .width = window.GetFramebufferSize().x,
        .height = window.GetFramebufferSize().y,
        .enable_vsync = m_enable_vsync,
    });

    std::filesystem::path path(ASSET_PATH "/shaders");
    m_shader_compiler = std::make_unique<ShaderCompiler>(path);

    CreateObjects();
}

Renderer::~Renderer() {
    std::cout << "Destroying Renderer\n";

    DestroyObjects();

    m_swapchain.reset();
    m_device.reset();
}

void Renderer::DrawFrame() {
    FrameContext &frame = m_frame_data[m_frame_index];

    frame.graphics_submit_fence->Wait();
    auto image_index = m_swapchain->AcquireNextImage(nullptr, frame.image_available->Handle());
    if (!image_index) {
        RecreateSwapChain();
        return;
    }

    SwapChainContext &swapchain_context = m_swapchain_data[*image_index];

    frame.graphics_submit_fence->Reset();
    frame.command_pool->Reset();

    RecordCommands(frame);

    QueueSubmitInfo submit_info {};
    submit_info.wait_semaphores.push_back({
        .semaphore = frame.image_available.get(),
        .value = 0,
        .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    });

    submit_info.signal_semaphores.push_back({
        .semaphore = swapchain_context.render_finished.get(),
        .value = 0,
        .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    });

    submit_info.command_buffers.push_back(frame.command_buffer.get());

    m_device->QueueSubmit(QueueType::Graphics, submit_info, *frame.graphics_submit_fence);

    std::vector<VkSemaphore> present_waits { swapchain_context.render_finished->Handle() };
    if (!m_swapchain->Present(present_waits))
        RecreateSwapChain();

    // vkDeviceWaitIdle(m_device->Device());

    m_frame_index = (m_frame_index + 1) % MaxFramesInFlight;
}

void Renderer::CreateObjects() {
    m_frame_data.resize(MaxFramesInFlight);
    for (uint32_t i = 0; i < MaxFramesInFlight; ++i) {
        FrameContext &frame = m_frame_data[i];
        
        frame.command_pool = m_device->CreateCommandPool(QueueType::Graphics, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
        frame.command_pool->SetDebugName(std::format("Render Command Pool [{}]", i));
        
        frame.command_buffer = frame.command_pool->AllocateCommandBuffer();
        frame.command_buffer->SetDebugName(std::format("Render Command Buffer [{}]", i));

        frame.graphics_submit_fence = m_device->CreateFence();
        frame.graphics_submit_fence->SetDebugName(std::format("Graphics Submit Fence [{}]", i));
        
        frame.image_available = m_device->CreateBinarySemaphore();
        frame.image_available->SetDebugName(std::format("Image Available Semaphore [{}]", i));
    }

    m_triangle_shader = m_shader_compiler->Compile("triangle");
    Assert(m_triangle_shader, "Failed to compile triangle shader!");

    CreateTrianglePipeline();
    CreateSwapChainObjects();
}

void Renderer::DestroyObjects() {
    if (m_device)
        vkDeviceWaitIdle(m_device->Device());

    DestroyTrianglePipeline();
    DestroySwapChainObjects();

    m_frame_data.clear();
}

void Renderer::RecordCommands(const FrameContext &frame) {
    CommandBuffer &cmd = *frame.command_buffer;
    VulkanImage &swapchain_image = m_swapchain->CurrentImage();
    SwapChainContext &context = m_swapchain_data[m_swapchain->CurrentImageIndex()];

    glm::vec4 clear_color = glm::vec4(0.02f, 0.02f, 0.025f, 1.0f);
    const VkExtent3D extent = swapchain_image.Extent();

    cmd.Begin();
    cmd.BeginLabel("Drawing to image", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    {
        cmd.TransitionLayout(*context.draw_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);        
        cmd.BeginRendering({
            ImageAttachment{
                .type = AttachmentType::Color,
                .image = *context.draw_image,
            }
        }, clear_color);
            cmd.BindGraphicsPipeline(m_triangle_pipeline);
            cmd.SetViewportAndScissor(glm::ivec2(0), glm::uvec2(extent.width, extent.height));
            cmd.Draw(3);
        cmd.EndRendering();
    }
    cmd.EndLabel();

    cmd.BeginLabel("Copying image to swapchain", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    {
        cmd.TransitionLayout(*context.draw_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        cmd.TransitionLayout(swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        cmd.CopyImage(*context.draw_image, swapchain_image);

        cmd.TransitionLayout(swapchain_image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    }
    cmd.EndLabel();
    cmd.End();
}

void Renderer::CreateSwapChainObjects() {
    m_swapchain_data.resize(m_swapchain->GetImageCount());

    const VkExtent3D &extent = m_swapchain->CurrentImage().Extent();
    for (uint32_t i = 0; i < m_swapchain->GetImageCount(); ++i) {
        SwapChainContext &context = m_swapchain_data[i];

        context.render_finished = m_device->CreateBinarySemaphore();
        context.render_finished->SetDebugName(std::format("Render Finished Semaphore [{}]", i));

        context.draw_image = VulkanImage::ImageBuilder()
            .Image2D(extent.width, extent.height)
            .Format(m_swapchain->CurrentImage().Format())
            .AddUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .AddUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            .Build(*m_device);
        context.draw_image->SetDebugName(std::format("Draw Image [{}]", i));
    }
}

void Renderer::DestroySwapChainObjects() {
    m_swapchain_data.clear();
}

void Renderer::CreateTrianglePipeline() {
    VkPipelineLayoutCreateInfo layout_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };

    VK_CHECK(vkCreatePipelineLayout(
        m_device->Device(),
        &layout_create_info,
        nullptr,
        &m_triangle_pipeline_layout
    ));

    VulkanImage &swapchain_image = m_swapchain->CurrentImage();

    Assert(m_triangle_shader, "Triangle shader was not compiled!");

    m_triangle_pipeline = PipelineBuilder_Graphics(*m_device, m_triangle_pipeline_layout)
        .AddShader(*m_triangle_shader)
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .DisableCulling()
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .DisableMSAA()
        .DisableBlending()
        .SetLineWidth(1.0f)
        .AddColorAttachmentFormat(swapchain_image.Format())
        .Build();

    m_device->SetDebugName(m_triangle_pipeline_layout, "Triangle Pipeline Layout");
    m_device->SetDebugName(m_triangle_pipeline, "Triangle Pipeline");
}

void Renderer::DestroyTrianglePipeline() {
    if (m_triangle_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device->Device(), m_triangle_pipeline, nullptr);
        m_triangle_pipeline = VK_NULL_HANDLE;
    }

    if (m_triangle_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device->Device(), m_triangle_pipeline_layout, nullptr);
        m_triangle_pipeline_layout = VK_NULL_HANDLE;
    }
}

void Renderer::RecreateSwapChain() {
    const glm::ivec2 framebuffer_size = m_window.GetFramebufferSize();

    if (framebuffer_size.x == 0 || framebuffer_size.y == 0)
        return;

    vkDeviceWaitIdle(m_device->Device());

    DestroySwapChainObjects();

    m_swapchain->Recreate(SwapChainConfig {
        .width = static_cast<uint32_t>(framebuffer_size.x),
        .height = static_cast<uint32_t>(framebuffer_size.y),
        .enable_vsync = m_enable_vsync,
    });

    CreateSwapChainObjects();
}

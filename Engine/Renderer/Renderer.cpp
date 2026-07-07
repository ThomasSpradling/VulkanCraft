#include "Renderer.h"
#include "Platform/Graphics/Common.h"
#include "Platform/Graphics/PipelineBuilder.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanDevice.h"
#include <filesystem>
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

    frame.frame_complete_fence->Wait();
    auto image_index = m_swapchain->AcquireNextImage(nullptr, frame.image_available->Handle());
    if (!image_index) {
        RecreateSwapChain();
        return;
    }

    SwapChainContext &swapchain_context = m_swapchain_data[*image_index];

    frame.frame_complete_fence->Reset();
    frame.command_pool->Reset();

    RecordCommands(frame);

    QueueSubmitInfo submit_info {};
    submit_info.wait_semaphores.push_back({
        .semaphore = frame.image_available.get(),
        .value = 0,
        .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    });

    submit_info.signal_semaphores.push_back({
        .semaphore = swapchain_context.render_finished.get(),
        .value = 0,
        .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    });

    submit_info.command_buffers.push_back(frame.command_buffer.get());

    m_device->QueueSubmit(QueueType::Graphics, submit_info, *frame.frame_complete_fence);

    std::vector<VkSemaphore> present_waits { swapchain_context.render_finished->Handle() };
    if (!m_swapchain->Present(present_waits))
        RecreateSwapChain();

    m_frame_index = (m_frame_index + 1) % MaxFramesInFlight;
}

void Renderer::CreateObjects() {
    m_frame_data.resize(MaxFramesInFlight);
    for (uint32_t i = 0; i < MaxFramesInFlight; ++i) {
        FrameContext &frame = m_frame_data[i];
        
        frame.command_pool          = m_device->CreateCommandPool(QueueType::Graphics, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
        frame.command_buffer        = frame.command_pool->AllocateCommandBuffer();
        frame.frame_complete_fence  = m_device->CreateFence();
        frame.image_available       = m_device->CreateBinarySemaphore();
    }

    m_triangle_shader = m_shader_compiler->Compile("triangle");
    Assert(m_triangle_shader, "Failed to compile triangle shader!");

    CreateSwapChainObjects();
}

void Renderer::DestroyObjects() {
    if (m_device)
        vkDeviceWaitIdle(m_device->Device());

    DestroySwapChainObjects();

    m_frame_data.clear();
}

void Renderer::RecordCommands(const FrameContext &frame) {
    CommandBuffer &cmd = *frame.command_buffer;
    VulkanImage &swapchain_image = m_swapchain->CurrentImage();

    VkClearValue clear_value {
        .color = {
            .float32 = { 0.02f, 0.02f, 0.025f, 1.0f },
        },
    };

    const VkExtent3D extent = swapchain_image.Extent();

    cmd.Begin();
        cmd.TransitionLayout(swapchain_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        cmd.BeginRendering(swapchain_image, clear_value);
            cmd.BindGraphicsPipeline(m_triangle_pipeline);
            cmd.SetViewportAndScissor({
                .width = extent.width,
                .height = extent.height,
            });
            cmd.Draw(3);
        cmd.EndRendering();
        cmd.TransitionLayout(swapchain_image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    cmd.End();
}

void Renderer::CreateSwapChainObjects() {
    m_swapchain_data.resize(m_swapchain->GetImageCount());

    for (uint32_t i = 0; i < m_swapchain->GetImageCount(); ++i) {
        SwapChainContext &context = m_swapchain_data[i];

        context.render_finished = m_device->CreateBinarySemaphore();
    }

    CreateTrianglePipeline();
}

void Renderer::DestroySwapChainObjects() {
    DestroyTrianglePipeline();
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

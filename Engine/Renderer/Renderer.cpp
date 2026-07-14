#include "Renderer.h"
#include "Common.h"
#include "GLTFModel.h"
#include "GPUResourceManager.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Platform/Graphics/Common.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanDevice.h"
#include "Platform/Graphics/VulkanImage.h"
#include <chrono>
#include <filesystem>
#include <format>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <optional>
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
    m_resource_manager = std::make_unique<GPUResourceManager>(*m_device);

    CreateObjects();
}

Renderer::~Renderer() {
    std::cout << "Destroying Renderer\n";

    DestroyObjects();

    m_resource_manager.reset();
    m_swapchain.reset();
    m_device.reset();
}

void Renderer::EnableVSync() {
    if (m_enable_vsync)
        return;

    std::cout << "Enabled VSync\n";

    m_enable_vsync = true;
    RecreateSwapChain();
}

void Renderer::DisableVSync() {
    if (!m_enable_vsync)
        return;

    std::cout << "Disabled VSync\n";

    m_enable_vsync = false;
    RecreateSwapChain();
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

    UpdateSceneData();
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

    m_frame_index = (m_frame_index + 1) % MaxFramesInFlight;
}

void Renderer::DrawGLTF(GLTFModel &model, const CommandBuffer &cmd) {
    m_push_constant.material_buffer = model.MaterialBuffer().DeviceAddress();
    m_push_constant.vertex_buffer = model.VertexBuffer().DeviceAddress();

    cmd.BindIndexBuffer(model.IndexBuffer().Buffer());

    model.ForEachNode([&](GLTFModel::Node &node) {
        if (!node.mesh)
            return;

        m_push_constant.model = node.ComputeGlobalTransform();
        for (auto &primitive : node.mesh->primitives) {
            m_push_constant.material_id = primitive.material_index;
            cmd.PushConstants(m_triangle_pipeline->Layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_push_constant);
            cmd.DrawIndexed(primitive.index_count, 1, primitive.start_index);
        }
    });
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

        frame.scene_uniform_buffer = VulkanBuffer::BufferBuilder(*m_device)
            .AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            .AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
            .Size(sizeof(SceneUniformData))
            .Build();

        m_push_constant.scene_data_buffer = frame.scene_uniform_buffer->DeviceAddress();
    }

    m_triangle_shader = m_shader_compiler->Compile("GLTF/GLTFModel");
    Assert(m_triangle_shader, "Failed to compile shader!");

    CreateTrianglePipeline();
    CreateSwapChainObjects();

    // Hardcoded Objects
    std::filesystem::path gltf_path(ASSET_PATH "/models/InterpolationTest.glb");
    m_model = std::make_unique<GLTFModel>(*m_device, *m_resource_manager, gltf_path);
}

void Renderer::DestroyObjects() {
    if (m_device)
        vkDeviceWaitIdle(m_device->Device());

    m_model.reset();

    DestroyTrianglePipeline();
    DestroySwapChainObjects();

    m_frame_data.clear();
}

void Renderer::UpdateSceneData() {
    const VkExtent3D extent = m_swapchain->CurrentImage().Extent();

    FrameContext &frame = m_frame_data[m_frame_index];

    static auto previous_time = std::chrono::steady_clock::now();
    [[maybe_unused]] static float animation_time = 0.0f;

    const auto current_time = std::chrono::steady_clock::now();
    const std::chrono::duration<float> delta = current_time - previous_time;
    previous_time = current_time;

    animation_time += delta.count();

    for (uint32_t i = 0; i < 9; ++i) {
        m_model->PlayAnimation(i, animation_time, true);
    }

    //// Camera ////
    m_camera.SetAspect(extent.width, extent.height);

    //// Update Uniform Buffer ////
    auto *data = frame.scene_uniform_buffer->Mapped<SceneUniformData>();
    data->projection = m_camera.ComputeProjectionMatrix();
    data->view = m_camera.ComputeViewMatrix();
    data->sun_direction = glm::vec4(1.0, -2.0, -1.0, 2.0);
    data->ambient = 0.1f;
    m_push_constant.scene_data_buffer = frame.scene_uniform_buffer->DeviceAddress();
}

void Renderer::RecordCommands(const FrameContext &frame) {
    CommandBuffer &cmd = *frame.command_buffer;
    VulkanImage &swapchain_image = m_swapchain->CurrentImage();
    SwapChainContext &context = m_swapchain_data[m_swapchain->CurrentImageIndex()];

    auto clear_color = glm::vec4(0.25f, 0.05f, 0.8f, 1.0f);
    const VkExtent3D extent = swapchain_image.Extent();

    cmd.Begin();
        cmd.SetViewportAndScissor(glm::ivec2(0), glm::uvec2(extent.width, extent.height));

        cmd.BindDescriptorSet(0, *m_triangle_pipeline, m_resource_manager->GlobalDescriptorSet());

        cmd.TransitionLayout(*context.draw_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        cmd.TransitionLayout(*context.depth_buffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR);

        {
            std::vector<ImageAttachment> attachments {};
            attachments.push_back({
                .type = AttachmentType::Color,
                .image = *context.draw_image,
                .should_clear = true,
                .clear_color = clear_color,
            });
            attachments.push_back({ .type = AttachmentType::Depth, .image = *context.depth_buffer });

            cmd.BeginLabel("Draw Model", glm::vec4(0.2f, 0.8f, 0.2f, 1.0f));
            cmd.BindPipeline(*m_triangle_pipeline);
            cmd.BeginRendering(attachments);
                DrawGLTF(*m_model, cmd);
            cmd.EndRendering();
            cmd.EndLabel();

            // attachments[0].should_clear = false;
            // attachments[1].should_clear = false;
            // cmd.BeginLabel("Draw Wireframe", glm::vec4(0.4f, 0.4f, 0.4f, 1.0f));
            // cmd.BindPipeline(*m_wireframe_pipeline);
            // cmd.BeginRendering(attachments);
            //     DrawGLTF(*m_model, cmd);
            // cmd.EndRendering();
            // cmd.EndLabel();
        }

        cmd.BeginLabel("Copying image to swapchain", glm::vec4(0.0f, 0.8f, 0.0f, 1.0f));
        cmd.TransitionLayout(*context.draw_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        cmd.TransitionLayout(swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        cmd.CopyImage(*context.draw_image, swapchain_image);

        cmd.TransitionLayout(swapchain_image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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

        context.draw_image = VulkanImage::ImageBuilder(*m_device)
            .Image2D(extent.width, extent.height)
            .Format(m_device->GetLinearColorFormat())
            .AddUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .Build();
        context.draw_image->SetDebugName(std::format("Draw Image [{}]", i));

        context.depth_buffer = VulkanImage::ImageBuilder(*m_device)
            .Image2D(extent.width, extent.height)
            .Format(m_device->GetDepthOnlyFormat())
            .AddUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            .Build();
    }
}

void Renderer::DestroySwapChainObjects() {
    m_swapchain_data.clear();
}

void Renderer::CreateTrianglePipeline() {
    VkPushConstantRange range {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstantData),
    };

    VkSpecializationMapEntry entry_is_wireframe {
        .constantID = 0,
        .offset = offsetof(SpecializationConstantData, is_wireframe),
        .size = sizeof(m_specialization_constant.is_wireframe)
    };

    VkSpecializationInfo specialization_info {
        .mapEntryCount = 1,
        .pMapEntries = &entry_is_wireframe,
        .dataSize = sizeof(SpecializationConstantData),
        .pData = &m_specialization_constant,
    };

    Assert(m_triangle_shader, "Triangle shader was not compiled!");

    auto builder = VulkanPipeline::GraphicsBuilder(*m_device)
        .VertexShader(*m_triangle_shader, "main_vert")
        .FragmentShader(*m_triangle_shader, "main_frag")
        
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .EnableCulling()

        .EnableDepthTest()
        .AddColorAttachment({
            .format = m_device->GetLinearColorFormat(),
            .blending_mode = BlendMode::Disabled,
        })
        .SetDepthAttachmentFormat(m_device->GetDepthOnlyFormat())
        .AddPushConstant(range)
        .AddDescriptorSetLayout(m_resource_manager->GlobalDescriptorLayout())
        .SetSpecializationConstants(specialization_info);

    if (m_triangle_pipeline)
        builder.FromBase(m_triangle_pipeline->Pipeline());

    m_specialization_constant.is_wireframe = false;
    m_triangle_pipeline = builder.Build();

    builder.SetPolygonMode(VK_POLYGON_MODE_LINE)
        .SetDepthBias(-1.0f, 0.0f, -1.0f);
    
    m_specialization_constant.is_wireframe = true;
    m_wireframe_pipeline = builder.Build();

    m_triangle_pipeline->SetDebugName("Triangle Pipeline");
}

void Renderer::DestroyTrianglePipeline() {
    m_triangle_pipeline.reset();
    m_wireframe_pipeline.reset();
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

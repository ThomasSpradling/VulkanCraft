#include "Renderer.h"
#include "Common.h"
#include "GPUResourceManager.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Platform/Graphics/Common.h"
#include "Platform/Graphics/DescriptorLayoutBuilder.h"
#include "Platform/Graphics/DescriptorWriter.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanDevice.h"
#include "Platform/Graphics/VulkanImage.h"
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

        frame.scene_uniform_buffer = VulkanBuffer::BufferBuilder()
            .AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            .AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
            .Size(sizeof(SceneUniformData))
            .Build(*m_device);

        m_push_constant.scene_data_buffer = frame.scene_uniform_buffer->DeviceAddress();
    }

    m_material_buffer = VulkanBuffer::BufferBuilder()
        .AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Size(sizeof(MaterialData) * MaxMaterialCount)
        .Build(*m_device);
    m_push_constant.material_buffer = m_material_buffer->DeviceAddress();

    // Add some materials:

    std::array<glm::u8vec4, 1> white_pixels{
        glm::u8vec4(255, 255, 255, 255)
    };

    m_white_image = VulkanImage::ImageBuilder()
        .Image2D(1, 1)
        .Format(VK_FORMAT_R8G8B8A8_UNORM)
        .AddUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .Build(*m_device);
    m_white_image->TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_white_image->Upload(white_pixels.data(), sizeof(glm::u8vec4) * white_pixels.size());

    m_device->ImmediateSubmit(QueueType::Graphics, [&](const CommandBuffer &cmd) {
        cmd.ImageMemoryBarrier(*m_white_image)
            .DestAccess(VK_ACCESS_2_SHADER_SAMPLED_READ_BIT)
            .DestStage(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR)
            .TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .Execute();
    });
    
    TextureId white_id = m_resource_manager->AddTexture(*m_white_image);

    std::array<glm::u8vec4, 25> checkerboard_pixels;
    for (uint32_t y = 0; y < 5; ++y) {
        for (uint32_t x = 0; x < 5; ++x) {
            const bool white = ((x + y) % 2) == 0;

            checkerboard_pixels[y * 5 + x] =
                white
                    ? glm::u8vec4(255, 255, 255, 255)
                    : glm::u8vec4(0, 0, 0, 255);
        }
    }

    m_checker_image = VulkanImage::ImageBuilder()
        .Image2D(5, 5)
        .Format(VK_FORMAT_R8G8B8A8_UNORM)
        .MipMapLevels()
        .AddUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        .Build(*m_device);
    m_checker_image->TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_checker_image->Upload(checkerboard_pixels.data(), sizeof(glm::u8vec4) * checkerboard_pixels.size());

    m_device->ImmediateSubmit(QueueType::Graphics, [&](const CommandBuffer &cmd) {
        cmd.GenerateMipMaps(*m_checker_image, VK_FILTER_NEAREST);
        cmd.ImageMemoryBarrier(*m_checker_image)
            .DestAccess(VK_ACCESS_2_SHADER_SAMPLED_READ_BIT)
            .DestStage(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR)
            .TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .Execute();
    });
        
    TextureId checker_id = m_resource_manager->AddTexture(*m_checker_image);

    VkSamplerCreateInfo sampler_create_info {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,

        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,

        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,

        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,

        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,

        .minLod = 0.0f,
        .maxLod = 0.0f,

        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    vkCreateSampler(m_device->Device(), &sampler_create_info, nullptr, &m_sampler);
    SamplerId sampler_id = m_resource_manager->AddSampler(m_sampler);

    MaterialData white_material {
        .color_texture = white_id,
        .color_sampler = sampler_id,
        .base_color = glm::vec4(1.0f),
    };

    MaterialData red_material {
        .color_texture = checker_id,
        .color_sampler = sampler_id,
        .base_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
    };

    // Upload them:
    m_material_buffer->Upload<MaterialData>(&white_material, 0);
    m_material_buffer->Upload<MaterialData>(&red_material, 1);

    m_triangle_shader = m_shader_compiler->Compile("Mesh");
    Assert(m_triangle_shader, "Failed to compile shader!");

    // Vertex buffers
    
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    const auto V = [](glm::vec3 position, glm::vec2 uv, glm::vec3 normal, glm::vec4 color) {
        return MeshVertex {
            .position = position,
            .uv_x = uv.x,
            .normal = normal,
            .uv_y = uv.y,
            .color = color,
        };
    };

    const glm::vec4 red     { 1.0f, 0.0f, 0.0f, 1.0f };
    const glm::vec4 green   { 0.0f, 1.0f, 0.0f, 1.0f };
    const glm::vec4 blue    { 0.0f, 0.0f, 1.0f, 1.0f };
    const glm::vec4 yellow  { 1.0f, 1.0f, 0.0f, 1.0f };
    const glm::vec4 magenta { 1.0f, 0.0f, 1.0f, 1.0f };
    const glm::vec4 cyan    { 0.0f, 1.0f, 1.0f, 1.0f };

    vertices = {
        // Front +Z
        V({-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, red),
        V({ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, red),
        V({ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, red),
        V({-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, red),

        // Back -Z
        V({ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, green),
        V({-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, green),
        V({-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, green),
        V({ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, green),

        // Right +X
        V({ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, blue),
        V({ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, blue),
        V({ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, blue),
        V({ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, blue),

        // Left -X
        V({-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, yellow),
        V({-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, yellow),
        V({-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, yellow),
        V({-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, yellow),

        // Top +Y
        V({-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, magenta),
        V({ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, magenta),
        V({ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, magenta),
        V({-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, magenta),

        // Bottom -Y
        V({-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, cyan),
        V({ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, cyan),
        V({ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, cyan),
        V({-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, cyan),
    };

    indices = {
        0,  1,  2,   2,  3,  0,
        4,  5,  6,   6,  7,  4,
        8,  9, 10,  10, 11,  8,
        12, 13, 14,  14, 15, 12,
        16, 17, 18,  18, 19, 16,
        20, 21, 22,  22, 23, 20,
    };

    m_vertex_buffer = VulkanBuffer::BufferBuilder()
        .AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Size(sizeof(MeshVertex) * vertices.size())
        .Build(*m_device);
    m_index_buffer = VulkanBuffer::BufferBuilder()
        .AddUsage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
        .AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .Size(sizeof(uint32_t) * indices.size())
        .Build(*m_device);

    m_vertex_buffer->Upload(vertices);
    m_index_buffer->Upload(indices);

    m_push_constant.vertex_buffer = m_vertex_buffer->DeviceAddress();

    CreateTrianglePipeline();
    CreateSwapChainObjects();
}

void Renderer::DestroyObjects() {
    if (m_device)
        vkDeviceWaitIdle(m_device->Device());

    vkDestroySampler(m_device->Device(), m_sampler, nullptr);
    m_checker_image.reset();
    m_white_image.reset();

    m_vertex_buffer.reset();
    m_index_buffer.reset();
    m_material_buffer.reset();

    DestroyTrianglePipeline();
    DestroySwapChainObjects();

    m_frame_data.clear();
}

void Renderer::UpdateSceneData() {
    const VkExtent3D extent = m_swapchain->CurrentImage().Extent();
    m_camera.SetAspect(extent.width, extent.height);

    FrameContext &frame = m_frame_data[m_frame_index];

    auto *data = frame.scene_uniform_buffer->Mapped<SceneUniformData>();
    data->projection = m_camera.ComputeProjectionMatrix();
    data->view = m_camera.ComputeViewMatrix();
    data->sun_direction = glm::vec4(1.0, -2.0, -1.0, 2.0);
    data->ambient = 0.1f;
    m_push_constant.scene_data_buffer = frame.scene_uniform_buffer->DeviceAddress();

    using Clock = std::chrono::steady_clock;

    static const auto start_time = Clock::now();

    const auto now = Clock::now();
    const float time_seconds = std::chrono::duration<float>(now - start_time).count();

    m_push_constant.model = glm::rotate(
        glm::mat4(1.0f),
        time_seconds * glm::radians(90.0f),
        glm::normalize(glm::vec3(0.35f, 1.0f, 0.2f))
    );

    float integer;
    float decimal = std::modf(time_seconds, &integer);
    m_push_constant.material_id = (decimal < 0.5f) ? 0 : 1;
}

void Renderer::RecordCommands(const FrameContext &frame) {
    CommandBuffer &cmd = *frame.command_buffer;
    VulkanImage &swapchain_image = m_swapchain->CurrentImage();
    SwapChainContext &context = m_swapchain_data[m_swapchain->CurrentImageIndex()];

    auto clear_color = glm::vec4(0.25f, 0.05f, 0.8f, 1.0f);
    const VkExtent3D extent = swapchain_image.Extent();

    cmd.Begin();
        std::vector<ImageAttachment> attachments {};
        attachments.push_back({
            .type = AttachmentType::Color,
            .image = *context.draw_image,
            .should_clear = true,
            .clear_color = clear_color,
        });
        attachments.push_back({ .type = AttachmentType::Depth, .image = *context.depth_buffer });

        cmd.SetViewportAndScissor(glm::ivec2(0), glm::uvec2(extent.width, extent.height));
        // cmd.BindVertexBuffer(m_vertex_buffer->Buffer());
        cmd.BindIndexBuffer(m_index_buffer->Buffer());

        cmd.TransitionLayout(*context.draw_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        cmd.TransitionLayout(*context.depth_buffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR);

        cmd.BindDescriptorSet(0, *m_triangle_pipeline, m_resource_manager->GlobalDescriptorSet());

        cmd.BeginLabel("Cube Render", glm::vec4(0.2f, 0.2f, 0.5f, 1.0f));
        cmd.BeginRendering(attachments);
            cmd.BindPipeline(*m_triangle_pipeline);
            cmd.PushConstants(m_triangle_pipeline->Layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_push_constant);
            cmd.DrawIndexed(static_cast<uint32_t>(m_index_buffer->Size() / sizeof(uint32_t)));
        cmd.EndRendering();
        cmd.EndLabel();

        attachments[0].should_clear = false;
        cmd.BeginLabel("Wireframe Render", glm::vec4(0.4f, 0.4f, 0.4f, 1.0f));
        cmd.BeginRendering(attachments);
            cmd.BindPipeline(*m_wireframe_pipeline);
            cmd.DrawIndexed(static_cast<uint32_t>(m_index_buffer->Size() / sizeof(uint32_t)));
        cmd.EndRendering();
        cmd.EndLabel();

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

        context.draw_image = VulkanImage::ImageBuilder()
            .Image2D(extent.width, extent.height)
            .Format(m_device->GetColorFormat())
            .AddUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .Build(*m_device);
        context.draw_image->SetDebugName(std::format("Draw Image [{}]", i));

        context.depth_buffer = VulkanImage::ImageBuilder()
            .Image2D(extent.width, extent.height)
            .Format(m_device->GetDepthOnlyFormat())
            .AddUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            .Build(*m_device);
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
        .AddShader(*m_triangle_shader)
        // .AddBinding(0, sizeof(MeshVertex))
        // .AddAttribute(0, 0, DataFormat::Float3, offsetof(MeshVertex, position))
        // .AddAttribute(1, 0, DataFormat::Float, offsetof(MeshVertex, uv_x))
        // .AddAttribute(2, 0, DataFormat::Float3, offsetof(MeshVertex, normal))
        // .AddAttribute(3, 0, DataFormat::Float, offsetof(MeshVertex, uv_y))
        // .AddAttribute(4, 0, DataFormat::Float4, offsetof(MeshVertex, color))
        
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .EnableCulling()

        .EnableDepthTest()
        .AddColorAttachment({
            .format = m_device->GetColorFormat(),
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
        .DisableDepthTest()
        .SetDepthBias(0.0f, -1.0f, 0.0f);
    
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

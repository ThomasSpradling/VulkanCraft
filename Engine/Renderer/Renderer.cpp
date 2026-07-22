#include "Renderer.h"
#include "AssetManager/Buffer.h"
#include "AssetManager/GPUStructs.h"
#include "AssetManager/Material.h"
#include "Common.h"
#include "ImGuiRenderer.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Platform/Graphics/Common.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanDevice.h"
#include "Platform/Graphics/VulkanImage.h"
#include "Platform/Graphics/VulkanPipeline.h"
#include "World/DefaultComponents.h"
#include <chrono>
#include <filesystem>
#include <format>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <imgui.h>
#include <iostream>
#include <memory>
#include <optional>
#include <variant>
#include <vector>
#include "AssetManager/AssetManager.h"
#include "AssetManager/GLTFModel.h"
#include "Core/ToString.h"

Renderer::Renderer(VulkanDevice &device, const Window &window)
    : m_device(device)
    , m_window(window)
{
    std::cout << "Initializing Renderer\n";
    m_swapchain = std::make_unique<VulkanSwapChain>(m_device, SwapChainConfig{
        .width = window.GetFramebufferSize().x,
        .height = window.GetFramebufferSize().y,
        .enable_vsync = m_enable_vsync,
    });

    std::filesystem::path path(ASSET_PATH "/shaders");
    m_shader_compiler = std::make_unique<ShaderCompiler>(path);
    m_bindless_table = std::make_unique<BindlessDescriptorTable>(m_device);
}

Renderer::~Renderer() {
    std::cout << "Destroying Renderer\n";

    m_device.GetGarbageCollector().CollectAll();

    m_imgui_renderer.reset();
    DestroyObjects();

    m_bindless_table.reset();
    m_swapchain.reset();
}

void Renderer::Initialize() {
    CreateObjects();

    m_imgui_renderer = std::make_unique<ImGuiRenderer>(*this);
}

void Renderer::RenderScene(World &world, Entity camera, const SceneRenderOptions &options) {
    //// Prepare Frame ////
    FrameContext &frame = m_frame_data[m_frame_index];

    frame.graphics_submit_fence->Wait();
    m_device.GetGarbageCollector().Collect(m_frame_index);

    auto image_index = m_swapchain->AcquireNextImage(nullptr, frame.image_available->Handle());
    if (!image_index) {
        RecreateSwapChain();
        return;
    }
    
    frame.graphics_submit_fence->Reset();
    frame.command_pool->Reset();

    m_device.GetGarbageCollector().SetCurrentFrame(m_frame_index);
    
    SwapChainContext &swapchain_context = m_swapchain_data[*image_index];
    const VkExtent3D extent = m_swapchain->CurrentImage().Extent();

    //// Update Uniform Buffer ////

    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    glm::mat4 projection_matrix = CalculateProjectionMatrix(world.Get<CameraComponent>(camera), aspect);
    glm::mat4 camera_model = world.GlobalMatrix(camera);
    glm::mat4 view_matrix = glm::inverse(camera_model);

    VulkanBuffer &scene_buffer = m_asset_manager->GetBuffer(frame.scene_uniform_buffer);
    VulkanBuffer &light_data = m_asset_manager->GetBuffer(frame.light_data);
    VulkanBuffer &point_lights = m_asset_manager->GetBuffer(frame.point_lights);
    
    m_push_constant.scene_data_buffer = scene_buffer.DeviceAddress();
    {
        auto *data = scene_buffer.Mapped<SceneUniformData>();
        data->projection = projection_matrix;
        data->eye_position = glm::vec4(glm::vec3(camera_model[3]), 1.0f);
        data->view = view_matrix;
        data->sun_direction = glm::vec4(1.0, -2.0, -1.0, 2.0);
        data->ambient = m_environment_settings.ambient_color * m_environment_settings.ambient_intensity;
        data->light_data = light_data.DeviceAddress();
    }

    bool found = false;
    DirectionalLight directional_light;
    glm::vec3 light_direction;
    world.Each<DirectionalLight>([&](Entity entity, DirectionalLight &light) {
        if (found)
            return;
        directional_light = light;
        found = true;
        
        light_direction = glm::normalize(world.Get<Transform>(entity).rotation * glm::vec3(0.0f, 0.0f, -1.0f));
    });

    if (!found) {
        directional_light.color = glm::vec4(1.0f);
        directional_light.intensity = 1.0f;
        light_direction = glm::vec3(1.0f, -2.0f, -1.0f);
    }

    {
        auto *data = light_data.Mapped<GPULightData>();
        data->directional_light.color = directional_light.color;
        data->directional_light.color.w = directional_light.intensity;
        data->directional_light.direction = light_direction;
        data->point_lights = point_lights.DeviceAddress();

        auto *point_light_data = point_lights.Mapped<GPUPointLight>();
        uint32_t point_light_count = 0;
        world.Each<PointLight>([&](Entity entity, PointLight &light) {
            if (point_light_count >= MaxPointLights) {
                std::cerr << "Warning: Too many point lights! Not rendering anymore.\n";
                return;
            }
    
            GPUPointLight &gpu_light = point_light_data[point_light_count];
            gpu_light.color = light.color;
            gpu_light.color.w = light.intensity;
            gpu_light.range = light.range;
            gpu_light.position = glm::vec3(world.GlobalMatrix(entity)[3]);;
    
            point_light_count++;
        });
        data->point_light_count = point_light_count;
    }

    //// Draw Scene ////

    CommandBuffer &cmd = *frame.command_buffer;
    VulkanImage &swapchain_image = m_swapchain->CurrentImage();
    SwapChainContext &context = m_swapchain_data[m_swapchain->CurrentImageIndex()];

    auto clear_color = glm::vec4(0.25f, 0.05f, 0.8f, 1.0f);

    cmd.Begin();
    cmd.SetViewportAndScissor(glm::ivec2(0), glm::uvec2(extent.width, extent.height));

    cmd.TransitionLayout(*context.draw_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    cmd.TransitionLayout(*context.depth_buffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR);

    std::vector<ImageAttachment> attachments {};
    attachments.push_back({
        .type = AttachmentType::Color,
        .image = *context.draw_image,
        .should_clear = true,
        .clear_color = clear_color,
    });
    attachments.push_back({ .type = AttachmentType::Depth, .image = *context.depth_buffer });

    //// Draw Cube Map ////
    TextureId envmap_id = m_asset_manager->GetTexture(m_environment_map).first;

    cmd.BeginLabel("Sky Box", glm::vec4(1.0f, 1.0f, 0.8f, 1.0f));
    cmd.BindDescriptorSet(0, *m_skybox_pipeline, m_bindless_table->GlobalDescriptorSet());
    cmd.BindPipeline(*m_skybox_pipeline);
    cmd.BeginRendering({
        ImageAttachment {
            .type = AttachmentType::Color,
            .image = *context.draw_image,
            .should_clear = true,
            .clear_color = clear_color,
        },
        ImageAttachment {
            .type = AttachmentType::Depth,
            .image = *context.depth_buffer,
            .should_clear = true,
        }
    });
        const Mesh &mesh = m_asset_manager->GetMesh(m_skybox);
        glm::vec3 camera_translation = view_matrix[3];
        glm::mat4 camera_transform = glm::inverse(glm::translate(glm::mat4(1.0f), camera_translation)) * view_matrix;

        cmd.PushConstants(m_skybox_pipeline->Layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, SkyboxPushConstants {
            .model = glm::scale(glm::mat4(1.0f), glm::vec3(100.0f)),
            .view = camera_transform,
            .projection = projection_matrix,
            .vertex_buffer = mesh.VertexBuffer().DeviceAddress(),
            .cube_map_id = envmap_id,
        });
        cmd.BindIndexBuffer(mesh.IndexBuffer().Buffer());
        cmd.DrawIndexed(mesh.IndexCount());
    cmd.EndRendering();
    cmd.EndLabel();

    cmd.BeginLabel("Draw Scene");
    cmd.BindPipeline(*m_triangle_pipeline);
    cmd.BindDescriptorSet(0, *m_triangle_pipeline, m_bindless_table->GlobalDescriptorSet());
    cmd.BeginRendering({
        ImageAttachment {
            .type = AttachmentType::Color,
            .image = *context.draw_image,
            .should_clear = false,
            .clear_color = clear_color,
        },
        ImageAttachment {
            .type = AttachmentType::Depth,
            .image = *context.depth_buffer,
            .should_clear = false,
        }
    });
    m_push_constant.material_buffer = m_asset_manager->MaterialBuffer().DeviceAddress();
    world.Each<ProceduralMeshComponent>([&](Entity entity, ProceduralMeshComponent &component) {
        if (!component.visible)
            return;

        auto &mesh = m_asset_manager->GetMesh(component.mesh);
        auto &material = m_asset_manager->GetMaterial(component.material);
        m_push_constant.vertex_buffer = mesh.VertexBuffer().DeviceAddress();

        cmd.BindIndexBuffer(mesh.IndexBuffer().Buffer());

        m_push_constant.model = world.GlobalMatrix(entity);
        m_push_constant.material_id = material.material_index;
        m_push_constant.envmap_id = envmap_id;

        cmd.PushConstants(m_triangle_pipeline->Layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_push_constant);
        cmd.DrawIndexed(mesh.IndexCount());
    });

    world.Each<GLTFMeshComponent>([&](Entity entity, GLTFMeshComponent &component) {
        if (!component.visible)
            return;

        auto &gltf = m_asset_manager->GetGLTF(component.gltf);
        m_push_constant.vertex_buffer = gltf.VertexBuffer().DeviceAddress();
        cmd.BindIndexBuffer(gltf.IndexBuffer().Buffer());

        const GLTFModel::Node &node = gltf.GetNode(component.node_index);
        if (!node.mesh)
            return;
        m_push_constant.model = world.GlobalMatrix(entity);

        for (const GLTFModel::Primitive &primitive : node.mesh->primitives) {
            m_push_constant.material_id = primitive.material_index;
            cmd.PushConstants(m_triangle_pipeline->Layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, m_push_constant);
            cmd.DrawIndexed(primitive.index_count, 1, primitive.start_index);
        }
    });
    cmd.EndRendering();
    cmd.EndLabel();

    //// Draw ImGui ////

    cmd.BeginLabel("Render ImGui");
    m_imgui_renderer->BeginFrame();

    m_render_ui();



    // ImGui::ShowDemoWindow();

    // // auto [texture_id, texture] = m_asset_manager->GetTexture(m_environment_map);
    // // float width = static_cast<float>(texture.get().Extent().width);
    // // float height = static_cast<float>(texture.get().Extent().height);
    // VulkanImage &image = m_bindless_table->GetTexture(1);
    // ImGui::Image(ImTextureRef(ImTextureID(1)), ImVec2(static_cast<float>(image.Extent().width), static_cast<float>(image.Extent().height)));

    m_imgui_renderer->EndFrame(cmd, {
        .type = AttachmentType::Color,
        .image = *context.draw_image,
        .should_clear = false,
    });
    cmd.EndLabel();
    
    //// Copy to Swap Chain ////

    cmd.BeginLabel("Copying image to swapchain", glm::vec4(0.0f, 0.8f, 0.0f, 1.0f));
        cmd.TransitionLayout(*context.draw_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        cmd.TransitionLayout(swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        cmd.CopyImage(*context.draw_image, swapchain_image);

        cmd.TransitionLayout(swapchain_image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        cmd.EndLabel();
    cmd.End();

    //// Prepare for Next Frame ////
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

    m_device.QueueSubmit(QueueType::Graphics, submit_info, *frame.graphics_submit_fence);

    std::vector<VkSemaphore> present_waits { swapchain_context.render_finished->Handle() };
    if (!m_swapchain->Present(present_waits))
        RecreateSwapChain();

    m_frame_index = (m_frame_index + 1) % MaxFramesInFlight;
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

void Renderer::CreateObjects() {
    m_frame_data.resize(MaxFramesInFlight);
    for (uint32_t i = 0; i < MaxFramesInFlight; ++i) {
        FrameContext &frame = m_frame_data[i];
        
        frame.command_pool = m_device.CreateCommandPool(QueueType::Graphics, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
        frame.command_pool->SetDebugName(std::format("Render Command Pool [{}]", i));
        
        frame.command_buffer = frame.command_pool->AllocateCommandBuffer();
        frame.command_buffer->SetDebugName(std::format("Render Command Buffer [{}]", i));

        frame.graphics_submit_fence = m_device.CreateFence();
        frame.graphics_submit_fence->SetDebugName(std::format("Graphics Submit Fence [{}]", i));
        
        frame.image_available = m_device.CreateBinarySemaphore();
        frame.image_available->SetDebugName(std::format("Image Available Semaphore [{}]", i));

        frame.scene_uniform_buffer = m_asset_manager->CreateBuffer(GPUBufferData {
            .usage = BufferUsageBits::Storage,
            .size = sizeof(SceneUniformData),
            .debug_name = std::format("Scene Buffer [{}]", i),
        });
    }

    m_skybox = m_asset_manager->CreateMesh(Shape::Cube(2.0f));
    m_environment_map = m_asset_manager->LoadTextureCubeFromEquirectangular(ASSET_PATH "/textures/cowboy_town_saloon_4k.hdr", TextureFormat::RGB32Float);

    CreateLights();

    m_triangle_shader = m_shader_compiler->Compile("GLTF/GLTFModel");
    Assert(m_triangle_shader, "Failed to compile shader!");
    
    m_skybox_shader = m_shader_compiler->Compile("SkyBox/SkyBox");
    Assert(m_skybox_shader, "Failed to compile shader!");

    CreateTrianglePipeline();
    CreateSwapChainObjects();
}

void Renderer::DestroyObjects() {
    DestroyTrianglePipeline();
    DestroySwapChainObjects();

    m_frame_data.clear();
}

void Renderer::CreateSwapChainObjects() {
    m_swapchain_data.resize(m_swapchain->GetImageCount());

    const VkExtent3D &extent = m_swapchain->CurrentImage().Extent();
    for (uint32_t i = 0; i < m_swapchain->GetImageCount(); ++i) {
        SwapChainContext &context = m_swapchain_data[i];

        context.render_finished = m_device.CreateBinarySemaphore();
        context.render_finished->SetDebugName(std::format("Render Finished Semaphore [{}]", i));

        context.draw_image = VulkanImage::ImageBuilder(m_device)
            .Image2D(extent.width, extent.height)
            .Format(m_device.GetLinearColorFormat())
            .AddUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .Build();
        context.draw_image->SetDebugName(std::format("Draw Image [{}]", i));

        context.depth_buffer = VulkanImage::ImageBuilder(m_device)
            .Image2D(extent.width, extent.height)
            .Format(m_device.GetDepthOnlyFormat())
            .AddUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            .Build();
        context.depth_buffer->SetDebugName(std::format("Depth Image [{}]", i));
    }
}

void Renderer::DestroySwapChainObjects() {
    m_swapchain_data.clear();
}

void Renderer::CreateTrianglePipeline() {
    // Triangle Pipeline
    {
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
    
        auto builder = VulkanPipeline::GraphicsBuilder(m_device)
            .VertexShader(*m_triangle_shader, "main_vert")
            .FragmentShader(*m_triangle_shader, "main_frag")
            
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .EnableCulling()
    
            .EnableDepthTest()
            .AddColorAttachment({
                .format = m_device.GetLinearColorFormat(),
                .blending_mode = BlendMode::Disabled,
            })
            .SetDepthAttachmentFormat(m_device.GetDepthOnlyFormat())
            .AddPushConstant(range)
            .AddDescriptorSetLayout(m_bindless_table->GlobalDescriptorLayout())
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

    // Skybox Pipeline
    {
        VkPushConstantRange range {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(SkyboxPushConstants),
        };

        m_skybox_pipeline = VulkanPipeline::GraphicsBuilder(m_device)
            .VertexShader(*m_skybox_shader, "main_vert")
            .FragmentShader(*m_skybox_shader, "main_frag")
            .AddColorAttachment(ColorAttachment {
                .format = m_device.GetLinearColorFormat(),
                .blending_mode = BlendMode::Disabled,
            })
            .SetDepthAttachmentFormat(m_device.GetDepthOnlyFormat())
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .EnableDepthTest()
            .AddPushConstant(range)
            .AddDescriptorSetLayout(m_bindless_table->GlobalDescriptorLayout())
            .Build();
        m_skybox_pipeline->SetDebugName("SkyBox Pipeline");
    }
}

void Renderer::DestroyTrianglePipeline() {
    m_triangle_pipeline.reset();
    m_wireframe_pipeline.reset();
}

void Renderer::RecreateSwapChain() {
    const glm::ivec2 framebuffer_size = m_window.GetFramebufferSize();

    if (framebuffer_size.x == 0 || framebuffer_size.y == 0)
        return;

    vkDeviceWaitIdle(m_device.Device());

    DestroySwapChainObjects();

    m_swapchain->Recreate(SwapChainConfig {
        .width = static_cast<uint32_t>(framebuffer_size.x),
        .height = static_cast<uint32_t>(framebuffer_size.y),
        .enable_vsync = m_enable_vsync,
    });

    CreateSwapChainObjects();
}

void Renderer::CreateLights() {
    for (uint32_t i = 0; i < MaxFramesInFlight; ++i) {
        FrameContext &frame = m_frame_data[i];

        frame.light_data = m_asset_manager->CreateBuffer(GPUBufferData {
            .usage = BufferUsageBits::Storage,
            .size = sizeof(GPULightData),
            .debug_name = std::format("Light Data [{}]", i),
        });
        
        frame.point_lights = m_asset_manager->CreateBuffer(GPUBufferData {
            .usage = BufferUsageBits::Storage,
            .size = sizeof(GPUPointLight) * MaxPointLights,
            .debug_name = std::format("Point Lights [{}]", i),
        });
    }
}

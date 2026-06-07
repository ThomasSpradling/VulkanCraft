#include "../Graphics/utils.h"

// #include "../backend/resource_manager/text_resource.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include "../Events/WindowEvents/key_events.h"
#include "../Events/WindowEvents/mouse_events.h"
#include "../Events/event_dispatcher.h"
#include "../Graphics/descriptor_layout_builder.h"
#include "../Graphics/descriptor_writer.h"

#include "../Graphics/pipeline_builder.h"
#include <vulkan/vulkan_core.h>
#include "vulkan_craft.h"
#include <iostream>
#include <vector>

void VulkanCraft::Initialize() {
    std::cout << "Game initialized!" << std::endl;
    m_event_handler->AddListener(this);

    m_frame_data.resize(m_renderer->SwapChainImageCount());

    // Set up descriptor allocator
    m_descriptor_allocator = std::make_unique<DescriptorAllocator>(m_renderer->GetDevice());

    InitRenderTargets();
    InitPipelines();
    InitTextures();
    InitGeometry();
}

void VulkanCraft::ShutDown() {
    std::cout << "Game shutting down!" << std::endl;
    m_event_handler->RemoveListener(this);

    DestroyGeometry();
    DestroyPipelines();
    DestroyRenderTargets();
    DestroyTextures();
}

void VulkanCraft::Update(float delta_time) {
    static float time = 0.0f;
    time += delta_time * 0.0005f;

    const auto extent = m_renderer->DrawExtent();

    float aspect =
        static_cast<float>(extent.width) /
        static_cast<float>(extent.height);

    const float radius = 5.0f;

    // glm::vec3 camera_pos = glm::vec3(
    //     std::cos(time) * radius,
    //     3.0f,
    //     std::sin(time) * radius
    // );
    glm::vec3 camera_pos = glm::vec3(0.0f, 0.0f, 3.0f);

    glm::mat4 view = glm::lookAt(
        camera_pos,
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    glm::mat4 projection = glm::perspective(
        glm::radians(60.0f),
        aspect,
        0.1f,
        100.0f
    );

    projection[1][1] *= -1.0f;

    m_scene_data.view = view;
    m_scene_data.projection = projection;
}

void VulkanCraft::Render(std::optional<Frame> frame, float delta_time) {
    uint32_t i = m_renderer->GetCurrentFrameIndex();

    if (!frame.has_value()) {
        // Window has been resized or modified
        DestroyRenderTargets();
        m_frame_data.resize(m_renderer->SwapChainImageCount());
        InitRenderTargets();
        return;
    }

    UpdateUniforms();

    TransitionImageLayout(frame->cmd, frame->image, IMAGE_SUBRESOURCE_RANGE_COLOR, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    TransitionImageLayout(frame->cmd, m_frame_data[i].depth_image, IMAGE_SUBRESOURCE_RANGE_DEPTH, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo color_attachment {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = frame->image_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    VkRenderingAttachmentInfo depth_attachment {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = m_frame_data[i].depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.depthStencil = { 1.0f, 0 }}
    };
    VkRenderingInfo rendering_info {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = frame->extent,
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
        .pDepthAttachment = &depth_attachment,
    };
    vkCmdBeginRendering(frame->cmd, &rendering_info);
    // Draw commands
    {
        
        VkViewport viewport {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(frame->extent.width),
            .height = static_cast<float>(frame->extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(frame->cmd, 0, 1, &viewport);
    
        VkRect2D scissors {
            .offset = { 0, 0 },
            .extent = frame->extent
        };
        vkCmdSetScissor(frame->cmd, 0, 1, &scissors);
    
        for (auto &render_object : m_render_objects) {
            vkCmdBindPipeline(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_object.pipeline);

            vkCmdBindDescriptorSets(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_object.pipeline_layout, 0, 1, &m_frame_data[i].global_descriptor_set, 0, nullptr);
            vkCmdBindDescriptorSets(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_object.pipeline_layout, 1, 1, &render_object.material_descriptor_set, 0, nullptr);

            m_push_constants.vertex_buffer = render_object.vertex_buffer;
            m_push_constants.model = render_object.transform;

            vkCmdPushConstants(frame->cmd, render_object.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &m_push_constants);
            vkCmdBindIndexBuffer(frame->cmd, render_object.index_buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(frame->cmd, render_object.index_count, 1, render_object.start_index, 0, 0);
        }
    }
    vkCmdEndRendering(frame->cmd);
}

void VulkanCraft::OnEvent(const Event &event) {
    EventDispatcher dispatcher(event);

    dispatcher.Dispatch<MouseMovedEvent>([this](const MouseMovedEvent &e) {
        glm::vec2 screen_size = m_application->GetWindowSize();
        glm::vec2 color = (e.GetMousePosition() / screen_size);
        m_current_color = { color.r, color.g, 0.0f };
    });
}

void VulkanCraft::InitRenderTargets() {
    // Depth Buffer

    for (uint32_t i = 0; i < m_frame_data.size(); ++i) {
        VkImageCreateInfo image_create_info {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = m_renderer->GetDepthOnlyFormat(),
            .extent = {
                .width = m_renderer->DrawExtent().width,
                .height = m_renderer->DrawExtent().height,
                .depth = 1,
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VmaAllocationCreateInfo allocation_create_info {
            .flags = 0,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        VK_CHECK(vmaCreateImage(m_renderer->GetMemoryAllocator(), &image_create_info, &allocation_create_info, &m_frame_data[i].depth_image, &m_frame_data[i].depth_image_alloc, nullptr));
    
        VkImageViewCreateInfo image_view_create_info {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_frame_data[i].depth_image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_renderer->GetDepthOnlyFormat(),
            .components = COMPONENT_MAPPING_DEFAULT,
            .subresourceRange = IMAGE_SUBRESOURCE_RANGE_DEPTH,
        };
        VK_CHECK(vkCreateImageView(m_renderer->GetDevice(), &image_view_create_info, nullptr, &m_frame_data[i].depth_image_view));
    }
}

void VulkanCraft::DestroyRenderTargets() {
    for (uint32_t i = 0; i < m_frame_data.size(); ++i) {
        vkDestroyImageView(m_renderer->GetDevice(), m_frame_data[i].depth_image_view, nullptr);
        vmaDestroyImage(m_renderer->GetMemoryAllocator(), m_frame_data[i].depth_image, m_frame_data[i].depth_image_alloc);
    }
}

void VulkanCraft::InitGeometry() {
    m_model = std::make_shared<GLTFModel>(*m_renderer, m_gltf_common_data, RESOURCE_PATH "/models/DamagedHelmet.glb");

    for (auto &mesh : m_model->GetMeshes()) {
        for (auto &primitive : mesh->primitives) {
            m_render_objects.push_back({
                .index_count = primitive.index_count,
                .start_index = primitive.start_index,
                .index_buffer = mesh->mesh_buffers.index_buffer,
                .transform = glm::mat4(1.0f),
                .vertex_buffer = mesh->mesh_buffers.device_address,

                .pipeline = primitive.pipeline,
                .pipeline_layout = primitive.pipeline_layout,

                .material_descriptor_set = primitive.material_descriptor_set,
            });
        }
    }
}

void VulkanCraft::DestroyGeometry() {
    // m_renderer->DestroyGPUMesh(m_mesh);
    m_model->CleanUp();
}

void VulkanCraft::InitTextures() {
}

void VulkanCraft::DestroyTextures() {
}

void VulkanCraft::InitPipelines() {
    //// ---- Initialize Global Descriptor Sets ---- ////
    std::vector<DescriptorPoolRatios> ratios {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
    };
    m_descriptor_allocator->Init(10, ratios);
    m_global_layout = DescriptorLayoutBuilder(m_renderer->GetDevice())
        .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .Build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    for (uint32_t i = 0; i < m_frame_data.size(); ++i) {
        m_frame_data[i].global_descriptor_set = m_descriptor_allocator->AllocateDescriptorSet(m_global_layout);
        
        // Create uniform buffer
        VkBufferCreateInfo buffer_create_info {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = sizeof(SceneData),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VmaAllocationCreateInfo allocation_create_info {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        VK_CHECK(vmaCreateBuffer(m_renderer->GetMemoryAllocator(), &buffer_create_info, &allocation_create_info, &m_frame_data[i].global_uniform_buffer, &m_frame_data[i].global_uniform_buffer_alloc, nullptr));
        
        // Attach uniform buffer to descriptor
        DescriptorWriter(m_renderer->GetDevice())
            .WriteBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {
                .buffer = m_frame_data[i].global_uniform_buffer,
                .offset = 0,
                .range = sizeof(SceneData),
            })
            .Write(m_frame_data[i].global_descriptor_set);
    }
    

    //// ---- Initialize GLTF Material Descriptor Sets ---- ////
    m_gltf_common_data.material_layout = DescriptorLayoutBuilder(m_renderer->GetDevice())
        .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        .AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        .Build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    //// ---- Initialize GLTF Material Pipelines ---- ////

    VkPushConstantRange push_constant_data {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstantData),
    };
    VkShaderModule vertex_shader_module = m_renderer->LoadShader(RESOURCE_PATH "/shaders/gltf/mesh.vert.spv");
    VkShaderModule fragment_shader_module = m_renderer->LoadShader(RESOURCE_PATH "/shaders/gltf/mesh.frag.spv");

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts {
        m_global_layout,
        m_gltf_common_data.material_layout,
    };
    std::vector<VkPushConstantRange> push_constant_ranges {
        push_constant_data
    };
    VkPipelineLayoutCreateInfo pipeline_layout_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size()),
        .pPushConstantRanges = push_constant_ranges.data(),
    };
    VK_CHECK(vkCreatePipelineLayout(m_renderer->GetDevice(), &pipeline_layout_create_info, nullptr, &m_gltf_common_data.opaque_pipeline_layout));

    m_gltf_common_data.opaque_pipeline = PipelineBuilder_Graphics(m_renderer->GetDevice(), m_gltf_common_data.opaque_pipeline_layout)
        .AddColorAttachmentFormat(m_renderer->GetColorFormat())
        .SetDepthAttachmentFormat(m_renderer->GetDepthOnlyFormat())
        .EnableDepthTest()        
        .AddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader_module)
        .AddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader_module)
        .Build();

    vkDestroyShaderModule(m_renderer->GetDevice(), vertex_shader_module, nullptr);
    vkDestroyShaderModule(m_renderer->GetDevice(), fragment_shader_module, nullptr);
}

void VulkanCraft::DestroyPipelines() {
    vkDestroyPipeline(m_renderer->GetDevice(), m_gltf_common_data.opaque_pipeline, nullptr);
    vkDestroyPipelineLayout(m_renderer->GetDevice(), m_gltf_common_data.opaque_pipeline_layout, nullptr);

    for (uint32_t i = 0; i < m_frame_data.size(); ++i) {
        vmaDestroyBuffer(m_renderer->GetMemoryAllocator(), m_frame_data[i].global_uniform_buffer, m_frame_data[i].global_uniform_buffer_alloc);
    }
    m_descriptor_allocator->Destroy();
    
    vkDestroyDescriptorSetLayout(m_renderer->GetDevice(), m_global_layout, nullptr);
    vkDestroyDescriptorSetLayout(m_renderer->GetDevice(), m_gltf_common_data.material_layout, nullptr);
}

void VulkanCraft::UpdateUniforms() {
    uint32_t i = m_renderer->GetCurrentFrameIndex();

    // Map GPU memory onto CPU and edit it
    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(m_renderer->GetMemoryAllocator(), m_frame_data[i].global_uniform_buffer_alloc, &allocation_info);
    SceneData *device_scene_data = static_cast<SceneData *>(allocation_info.pMappedData);
    device_scene_data->ambient_color = glm::vec4(0.01f);
    device_scene_data->sunlight_color = glm::vec4(1.0f);
    device_scene_data->sunlight_direction = glm::vec4(1.0f, -1.0f, 0.0f, 3.0f);
    device_scene_data->projection = m_scene_data.projection;
    device_scene_data->view = m_scene_data.view;
    device_scene_data->view_projection = m_scene_data.projection * m_scene_data.view;
}

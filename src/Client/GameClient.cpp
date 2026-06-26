#include "../Graphics/utils.h"
#include <array>
#include <ctime>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <string>
#include <variant>

// #include "../backend/resource_manager/text_resource.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include "../Events/WindowEvents/KeyEvents.h"
#include "../Events/WindowEvents/MouseEvents.h"
#include "../Events/EventDispatcher.h"
#include "../Graphics/DescriptorLayoutBuilder.h"
#include "../Graphics/DescriptorWriter.h"

#include "../Network/Packet.h"
#include "../Network/Protocol.h"
#include "../Network/Packets/SystemPackets.h"

#include "../Graphics/PipelineBuilder.h"
#include <vulkan/vulkan_core.h>
#include "GameClient.h"
#include <iostream>
#include <vector>

void GameClient::Initialize() {
    std::cout << "Game initialized!" << std::endl;
    m_frame_data.resize(m_renderer->SwapChainImageCount());

    // TODO: Remove
    srand(time(nullptr));

    // Graphics

    // Set up descriptor allocator
    m_descriptor_allocator = std::make_unique<DescriptorAllocator>(m_renderer->GetDevice());

    InitRenderTargets();
    InitPipelines();
    InitTextures();
    InitGeometry();

    std::cout << "Loaded stuff\n";

    InitClientSocket();
}

void GameClient::ShutDown() {
    m_client->Disconnect(m_server);

    std::cout << "Game shutting down!" << std::endl;

    DestroyGeometry();
    DestroyPipelines();
    DestroyRenderTargets();
    DestroyTextures();
}

void GameClient::Update(float delta_time) {
    //// Receive packets from server ////
    
    m_network_send_timer += delta_time;
    if (m_network_send_timer >= 1000.0f / NETWORK_INPUT_SEND_RATE) {
        m_network_send_timer = 0.0;

        SendNetworkPackets();
        m_client->Update(1000.0f / NETWORK_INPUT_SEND_RATE);
        ReceiveNetworkPackets();
    }

    const auto extent = m_renderer->DrawExtent();

    float aspect =
        static_cast<float>(extent.width) /
        static_cast<float>(extent.height);

    glm::mat4 projection = glm::perspective(
        glm::radians(60.0f),
        aspect,
        0.1f,
        10000.0f
    );
    projection[1][1] *= -1.0f;

    glm::vec3 forward;
    forward.x = cos(glm::radians(m_current_yaw)) * cos(glm::radians(m_current_pitch));
    forward.y = sin(glm::radians(m_current_pitch));
    forward.z = -sin(glm::radians(m_current_yaw)) * cos(glm::radians(m_current_pitch));
    forward = glm::normalize(forward);

    glm::vec3 player_forward(forward.x, 0.0f, forward.z);

    if (glm::length(player_forward) > 0.001f) {
        player_forward = glm::normalize(player_forward);
    } else {
        player_forward = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    m_player_direction = player_forward;

    glm::vec3 camera_position = m_current_position - player_forward * 5.0f + glm::vec3(0.0f, 1.0f, 0.0f) * 3.0f;
    glm::vec3 camera_target = m_current_position + glm::vec3(0.0f, 1.5f, 0.0f) + forward * 8.0f;

    glm::mat4 view = glm::lookAt(
        camera_position,
        camera_target,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    m_scene_data.view = view;
    m_scene_data.projection = projection;
}

void GameClient::Render(std::optional<Frame> frame, float delta_time) {
    uint32_t i = m_renderer->GetCurrentFrameIndex();

    if (!frame.has_value()) {
        // Window has been resized or modified
        DestroyRenderTargets();
        m_frame_data.resize(m_renderer->SwapChainImageCount());
        InitRenderTargets();
        return;
    }

    UpdateUniforms();
    
    m_draw_context.opaque_objects.clear();
    m_draw_context.transparent_objects.clear();
    
    for (uint32_t i = 0; i < m_players.size(); ++i) {
        glm::vec3 pos = m_players[i].position;
        float yaw = m_players[i].yaw;
        float pitch = m_players[i].pitch;
        glm::mat4 translate = glm::translate(pos);

        glm::mat4 yaw_rotate = glm::rotate(glm::mat4(1.0f), glm::radians(yaw + 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_model->Draw(m_draw_context, translate * yaw_rotate);
    }

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

        // Draw Chunks
        m_chunk_renderer->Draw(frame->cmd, m_frame_data[i].global_descriptor_set);
    
        // Draw GLTF Objects
        auto draw = [&](const RenderObject &render_object) {
            vkCmdBindPipeline(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_object.pipeline);

            vkCmdBindDescriptorSets(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_object.pipeline_layout, 0, 1, &m_frame_data[i].global_descriptor_set, 0, nullptr);
            vkCmdBindDescriptorSets(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_object.pipeline_layout, 1, 1, &render_object.descriptor_set, 0, nullptr);

            vkCmdBindIndexBuffer(frame->cmd, render_object.index_buffer, 0, VK_INDEX_TYPE_UINT32);

            m_push_constants.vertex_buffer = render_object.vertex_buffer;
            m_push_constants.model = render_object.transform;
            vkCmdPushConstants(frame->cmd, render_object.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &m_push_constants);
            
            vkCmdDrawIndexed(frame->cmd, render_object.index_count, 1, render_object.start_index, 0, 0);
        };
        
        for (auto &render_object : m_draw_context.opaque_objects)
            draw(render_object);
        for (auto &render_object : m_draw_context.transparent_objects)
            draw(render_object);

    }
    vkCmdEndRendering(frame->cmd);
}

void GameClient::InitRenderTargets() {
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

void GameClient::DestroyRenderTargets() {
    for (uint32_t i = 0; i < m_frame_data.size(); ++i) {
        vkDestroyImageView(m_renderer->GetDevice(), m_frame_data[i].depth_image_view, nullptr);
        vmaDestroyImage(m_renderer->GetMemoryAllocator(), m_frame_data[i].depth_image, m_frame_data[i].depth_image_alloc);
    }
}

void GameClient::InitGeometry() {
    m_model = std::make_shared<GLTFModel>(*m_renderer, m_gltf_common_data, ASSET_PATH "/models/DamagedHelmet.glb");
}

void GameClient::DestroyGeometry() {
    // m_renderer->DestroyGPUMesh(m_mesh);
    m_model->CleanUp();
}

void GameClient::InitTextures() {
}

void GameClient::DestroyTextures() {
}

void GameClient::InitPipelines() {
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
    VkShaderModule vertex_shader_module = m_renderer->LoadShader(ASSET_PATH "/shaders/gltf/mesh.vert.spv");
    VkShaderModule fragment_shader_module = m_renderer->LoadShader(ASSET_PATH "/shaders/gltf/mesh.frag.spv");

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
    m_gltf_common_data.transparent_pipeline_layout = m_gltf_common_data.opaque_pipeline_layout;

    m_gltf_common_data.opaque_pipeline = PipelineBuilder_Graphics(m_renderer->GetDevice(), m_gltf_common_data.opaque_pipeline_layout)
        .AddColorAttachmentFormat(m_renderer->GetColorFormat())
        .SetDepthAttachmentFormat(m_renderer->GetDepthOnlyFormat())
        .EnableDepthTest()
        .AddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader_module)
        .AddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader_module)
        .Build();

    m_gltf_common_data.transparent_pipeline = PipelineBuilder_Graphics(m_renderer->GetDevice(), m_gltf_common_data.transparent_pipeline_layout)
        .AddColorAttachmentFormat(m_renderer->GetColorFormat())
        .SetDepthAttachmentFormat(m_renderer->GetDepthOnlyFormat())
        .EnableDepthTest()
        .DisableDepthWrite()
        .EnableBlendingAlpha()
        .AddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader_module)
        .AddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader_module)
        .Build();

    vkDestroyShaderModule(m_renderer->GetDevice(), vertex_shader_module, nullptr);
    vkDestroyShaderModule(m_renderer->GetDevice(), fragment_shader_module, nullptr);

    m_chunk_renderer = std::make_unique<ChunkRenderer>(*m_renderer);
    m_chunk_renderer->Init(m_global_layout);
}

void GameClient::DestroyPipelines() {
    m_chunk_renderer->Destroy();

    vkDestroyPipeline(m_renderer->GetDevice(), m_gltf_common_data.opaque_pipeline, nullptr);
    vkDestroyPipeline(m_renderer->GetDevice(), m_gltf_common_data.transparent_pipeline, nullptr);
    vkDestroyPipelineLayout(m_renderer->GetDevice(), m_gltf_common_data.opaque_pipeline_layout, nullptr);

    for (uint32_t i = 0; i < m_frame_data.size(); ++i) {
        vmaDestroyBuffer(m_renderer->GetMemoryAllocator(), m_frame_data[i].global_uniform_buffer, m_frame_data[i].global_uniform_buffer_alloc);
    }
    m_descriptor_allocator->Destroy();
    
    vkDestroyDescriptorSetLayout(m_renderer->GetDevice(), m_global_layout, nullptr);
    vkDestroyDescriptorSetLayout(m_renderer->GetDevice(), m_gltf_common_data.material_layout, nullptr);
}

void GameClient::UpdateUniforms() {
    uint32_t i = m_renderer->GetCurrentFrameIndex();

    // Map GPU memory onto CPU and edit it
    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(m_renderer->GetMemoryAllocator(), m_frame_data[i].global_uniform_buffer_alloc, &allocation_info);
    SceneData *device_scene_data = static_cast<SceneData *>(allocation_info.pMappedData);
    device_scene_data->ambient_color = glm::vec4(0.2f);
    device_scene_data->sunlight_color = glm::vec4(1.0f);
    device_scene_data->sunlight_direction = glm::vec4(glm::normalize(glm::vec3(1.0f, -1.0f, -2.0f)), 3.0f);
    device_scene_data->projection = m_scene_data.projection;
    device_scene_data->view = m_scene_data.view;
    device_scene_data->view_projection = m_scene_data.projection * m_scene_data.view;
}

void GameClient::InitClientSocket() {
    m_socket_api = std::make_unique<SocketAPI>();
    m_socket_api->Initialize();
    
    m_client = std::make_unique<NetworkHost>(HostType::Client);
    NetworkAddress server_address {"127.0.0.1", PROTOCOL_PORT};

    if (auto peer = m_client->Connect(server_address)) {
        m_server = *peer;
        return;
    }

    std::cerr << "Could not connect to server address!\n";
}

void GameClient::DestroyClientSocket() {
    // m_client->Disconnect(m_server);
}

void GameClient::ReceiveNetworkPackets() {
    NetworkCommand command;
    while (m_client->PollNetworkCommand(command)) {
        if (command.type != NetworkCommandType::ReceivePacket)
            continue;
        
        Packet &packet = *command.packet;
        switch (packet.Type()) {
            case PacketType::Test: {
                auto *test_packet = dynamic_cast<TestPacket *>(&packet);
                std::cout << "Received packet from server: " << test_packet->value << "\n";
                break;
            }
            default:
                break;
        }
    }
}

void GameClient::SendNetworkPackets() {
    if (!m_client->IsConnected(m_server))
        return;

    static bool sent = false;

    if (!sent) {
        auto packet = std::make_unique<TestPacket>();
        // std::vector<uint32_t> items = ;
        // packet->items
        packet->value = std::rand();
        m_client->SendPacket(m_server, m_basic_channel, std::move(packet));
        sent = true;
    }
}

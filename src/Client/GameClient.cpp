#include "../Graphics/utils.h"
#include <array>
#include <optional>
#include <variant>

// #include "../backend/resource_manager/text_resource.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include "../Events/WindowEvents/KeyEvents.h"
#include "../Events/WindowEvents/MouseEvents.h"
#include "../Events/EventDispatcher.h"
#include "../Graphics/DescriptorLayoutBuilder.h"
#include "../Graphics/DescriptorWriter.h"

#include "../Network/Packets.h"

#include "../Graphics/PipelineBuilder.h"
#include <vulkan/vulkan_core.h>
#include "GameClient.h"
#include <iostream>
#include <vector>

void GameClient::Initialize() {
    std::cout << "Game initialized!" << std::endl;
    m_frame_data.resize(m_renderer->SwapChainImageCount());

    // Graphics

    // Set up descriptor allocator
    m_descriptor_allocator = std::make_unique<DescriptorAllocator>(m_renderer->GetDevice());

    InitRenderTargets();
    InitPipelines();
    InitTextures();
    InitGeometry();

    // m_player = std::make_unique<Player>();

    // Networks
    InitClientSocket();
}

void GameClient::ShutDown() {
    std::cout << "Game shutting down!" << std::endl;

    DestroyGeometry();
    DestroyPipelines();
    DestroyRenderTargets();
    DestroyTextures();
}

void GameClient::Update(float delta_time) {
    //// Receive packets from server ////
    ReceiveNetworkPackets();
    
    m_network_send_timer += delta_time;
    if (m_network_send_timer >= 1000.0f / NETWORK_INPUT_SEND_RATE) {
        m_network_send_timer = 0.0;
        SendNetworkPackets();
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

    glm::vec3 direction;
    direction.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    direction.y = sin(glm::radians(m_pitch));
    direction.z = -sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

    glm::vec3 front = glm::normalize(direction);
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::normalize(glm::cross(right, front));

    glm::mat4 view = glm::lookAt(
        m_current_position - front,
        m_current_position,
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
    m_model->Draw(m_draw_context);

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
    m_model = std::make_shared<GLTFModel>(*m_renderer, m_gltf_common_data, RESOURCE_PATH "/models/DamagedHelmet.glb");
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
    device_scene_data->sunlight_direction = glm::vec4(1.0f, -1.0f, 0.0f, 3.0f);
    device_scene_data->projection = m_scene_data.projection;
    device_scene_data->view = m_scene_data.view;
    device_scene_data->view_projection = m_scene_data.projection * m_scene_data.view;
}

void GameClient::InitClientSocket() {
    const std::string server_address = "127.0.0.1";

    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
        std::cerr << "WSA starup failed!";
    }

    addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };

    addrinfo *address_info;
    if (getaddrinfo(server_address.data(), "9999", &hints, &address_info) != 0) {
        std::cerr << "failed to create addrinfo\n";
    }

    m_socket = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Invalid socket!\n";
    }

    std::cout << "Set Client Sockets!\n";

    NetworkBuffer buffer;
    Packet send_packet {
        .packet_type = PacketType::Ack,
        // .packet_data = std::monostate{},
    };
    send_packet.Write(buffer);
    
    int bytes_sent = sendto(m_socket, buffer.GetData(), buffer.GetSize(), 0, address_info->ai_addr, address_info->ai_addrlen);
    if (bytes_sent == SOCKET_ERROR) {
        std::cerr << "Failed to send from client!\n";
    }
    buffer.Clear();
    
    // Receive an acknowledgement
    sockaddr from_addr;
    int from_length = sizeof(from_addr);

    NetworkBuffer recv_buffer;
    recv_buffer.Resize(512);
    int bytes_received = recvfrom(m_socket, recv_buffer.GetData(), recv_buffer.GetSize(), 0, &from_addr, &from_length);
    if (bytes_received == SOCKET_ERROR)
        std::cerr << "Error on recvfrom!\n";

    Packet recv_packet;
    recv_packet.Read(recv_buffer);
    if (recv_packet.packet_type == PacketType::Ack) {
        std::cout << "RECEIVED ACKNOWLEDGEMENT!\n";
        m_connected_server = true;
    }

    u_long non_blocking = 1;
    if (ioctlsocket(m_socket, FIONBIO, &non_blocking) == SOCKET_ERROR) {
        std::cerr << "Failed to set client socket non-blocking: " << WSAGetLastError() << "\n";
    }

    m_server_addrinfo = *address_info;
}

void GameClient::DestroyClientSocket() {
    
}

void GameClient::ReceiveNetworkPackets() {
    NetworkBuffer recv_buffer{};
    while (true) {
        recv_buffer.Resize(512);

        sockaddr from_addr{};
        int from_length = sizeof(from_addr);
        
        int bytes_received = recvfrom(m_socket, recv_buffer.GetData(), recv_buffer.GetSize(), 0, &from_addr, &from_length);
        if (bytes_received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
                break;
            std::cerr << "Client recvfrom failed!\n";
        }

        Packet packet{};
        packet.Read(recv_buffer);

        switch (packet.packet_type) {
            case PacketType::SetPosition: {
                const auto &packet_data = std::get<PacketSetPosition>(packet.packet_data);
                m_current_position = packet_data.position;
                break;
            }
            case PacketType::Ack:
            default:
                break;
        }

        recv_buffer.Clear();        
    }
}

void GameClient::SendNetworkPackets() {
    //// PLAYER INPUTS ////
    
    glm::vec3 player_direction = glm::vec3(0.0f);
    if (m_input_handler->IsKeyDown(GLFW_KEY_W)) {
        player_direction += glm::vec3(0.0f, 0.0f, -1.0f);
    } if (m_input_handler->IsKeyDown(GLFW_KEY_S))
        player_direction += glm::vec3(0.0f, 0.0f, 1.0f);
    if (m_input_handler->IsKeyDown(GLFW_KEY_D))
        player_direction += glm::vec3(1.0f, 0.0f, 0.0f);
    if (m_input_handler->IsKeyDown(GLFW_KEY_A))
        player_direction += glm::vec3(-1.0f, 0.0f, 0.0f);
    
    if (player_direction != glm::vec3(0.0f)) {
        player_direction = glm::normalize(player_direction);

        Packet packet {
            .packet_type = PacketType::MovePlayer,
            .packet_data = PacketMovePlayer {
                .direction = player_direction,
            },
        };
        
        NetworkBuffer send_buffer;
        packet.Write(send_buffer);

        int bytes = sendto(m_socket, send_buffer.GetData(), send_buffer.GetSize(), 0, m_server_addrinfo.ai_addr, m_server_addrinfo.ai_addrlen);
        if (bytes == SOCKET_ERROR) {
            std::cerr << "Failed sendto: " << WSAGetLastError() << "\n";
        }
    }
}

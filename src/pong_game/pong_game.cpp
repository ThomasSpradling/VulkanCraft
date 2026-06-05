#include "../utils/vulkan.h"

// #include "../backend/resource_manager/text_resource.h"
#include "../events/window_events/key_events.h"
#include "../events/window_events/mouse_events.h"
#include "../events/event_dispatcher.h"

#include "../backend/pipeline_builder.h"
#include "vulkan/vulkan_core.h"
#include "pong_game.h"
#include <iostream>
#include <vector>

void PongGame::Initialize() {
    std::cout << "Game initialized!" << std::endl;
    m_event_handler->AddListener(this);

    InitPipelines();
    InitGeometry();

    // auto text_handle = m_resource_manager->Load<TextResource>("hello_world");

    // std::cout << text_handle.Get()->Contents() << std::endl;
}

void PongGame::ShutDown() {
    std::cout << "Game shutting down!" << std::endl;
    m_event_handler->RemoveListener(this);

    DestroyGeometry();
    DestroyPipelines();
}

void PongGame::Update(float delta_time) {
    // auto *text = m_resource_manager->GetResource<TextResource>("hello_world");
    // std::cout << text->Contents() << std::endl;


}

void PongGame::Render(const Frame &frame, float delta_time) {
    uint32_t i = m_renderer->GetCurrentFrameIndex();
    UpdateUniforms();

    // Transition to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    {
        VkImageMemoryBarrier2 draw_image_barrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

    VkRenderingAttachmentInfo color_attachment {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = frame.image_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    VkRenderingInfo rendering_info {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = frame.extent,
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
    };
    vkCmdBeginRendering(frame.cmd, &rendering_info);
    // Draw commands
    {
        
        VkViewport viewport {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(frame.extent.width),
            .height = static_cast<float>(frame.extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(frame.cmd, 0, 1, &viewport);
    
        VkRect2D scissors {
            .offset = { 0, 0 },
            .extent = frame.extent
        };
        vkCmdSetScissor(frame.cmd, 0, 1, &scissors);
    
        // Bind graphics and draw
        vkCmdBindPipeline(frame.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_triangle_pipeline);
        vkCmdBindDescriptorSets(frame.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_triangle_pipeline_layout, 0, 1, &m_frame_data[i].view_descriptor_set, 0, nullptr);

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(frame.cmd, 0, 1, &m_vertex_buffer, offsets);
        vkCmdBindIndexBuffer(frame.cmd, m_index_buffer, 0, VK_INDEX_TYPE_UINT32);
        // vkCmdDraw(frame.cmd, 3, 1, 0, 0);
        vkCmdDrawIndexed(frame.cmd, 6, 1, 0, 0, 0);
    }
    vkCmdEndRendering(frame.cmd);
}

void PongGame::OnEvent(const Event &event) {
    EventDispatcher dispatcher(event);

    dispatcher.Dispatch<MouseMovedEvent>([this](const MouseMovedEvent &e) {
        glm::vec2 screen_size = m_application->GetWindowSize();
        glm::vec2 color = (e.GetMousePosition() / screen_size);
        m_current_color = { color.r, color.g, 0.0f };
    });
}

void PongGame::InitGeometry() {
    // Create raw vertex data
    std::vector<Vertex> vertices {
               // positions              // colors
        Vertex{glm::vec2(-0.8f, -0.8f),  glm::vec3(1.0f, 0.0f, 0.0f)},
        Vertex{glm::vec2( 0.8f, -0.8f),  glm::vec3(0.0f, 1.0f, 0.0f)},
        Vertex{glm::vec2( 0.8f,  0.8f),  glm::vec3(0.0f, 0.0f, 1.0f)},
        Vertex{glm::vec2(-0.8f,  0.8f),  glm::vec3(1.0f, 1.0f, 1.0f)},
        // Vertex{glm::vec2( 0.8f,  0.8f),  glm::vec3(0.0f, 0.0f, 1.0f)},
    };

    std::vector<uint32_t> indices {
        0, 1, 3,
        1, 2, 3
    };

    // Create vertex buffer
    {
        VkBufferCreateInfo buffer_create_info {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = vertices.size() * sizeof(Vertex),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // transfer_dst required for loading buffer data
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VmaAllocationCreateInfo allocation_create_info {
            .flags = 0,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        VK_CHECK(vmaCreateBuffer(m_renderer->GetMemoryAllocator(), &buffer_create_info, &allocation_create_info, &m_vertex_buffer, &m_vertex_buffer_alloc, nullptr));
        m_renderer->LoadBufferData(m_vertex_buffer, vertices);
    }
    
    // Create index buffer
    {
        VkBufferCreateInfo buffer_create_info {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = indices.size() * sizeof(uint32_t),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VmaAllocationCreateInfo allocation_create_info {
            .flags = 0,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        VK_CHECK(vmaCreateBuffer(m_renderer->GetMemoryAllocator(), &buffer_create_info, &allocation_create_info, &m_index_buffer, &m_index_buffer_alloc, nullptr));
        m_renderer->LoadBufferData(m_index_buffer, indices);
    }
}

void PongGame::DestroyGeometry() {
    vmaDestroyBuffer(m_renderer->GetMemoryAllocator(), m_vertex_buffer, m_vertex_buffer_alloc);
    vmaDestroyBuffer(m_renderer->GetMemoryAllocator(), m_index_buffer, m_index_buffer_alloc);
}

void PongGame::InitPipelines() {
    const uint32_t SWAPCHAIN_COUNT = m_renderer->SwapChainImageCount();
    m_frame_data.resize(SWAPCHAIN_COUNT);

    // Create uniform buffers for view-projection matrix
    {
        const uint32_t VIEW_BINDING = 0;
        const VkDeviceSize uniform_buffer_size = sizeof(UniformData); // TODO

        VkDescriptorSetLayoutBinding view_ubo_binding = {
            .binding = VIEW_BINDING,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };

        VkDescriptorSetLayoutCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &view_ubo_binding
        };
        VK_CHECK(vkCreateDescriptorSetLayout(m_renderer->GetDevice(), &create_info, nullptr, &m_view_uniform_layout));
        
        for (uint32_t i = 0; i < SWAPCHAIN_COUNT; ++i) {
            // Allocate descriptor set
            VkDescriptorSetAllocateInfo allocate_info {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = m_renderer->GetDescriptorPool(),
                .descriptorSetCount = 1,
                .pSetLayouts = &m_view_uniform_layout,
            };
            VK_CHECK(vkAllocateDescriptorSets(m_renderer->GetDevice(), &allocate_info, &m_frame_data[i].view_descriptor_set));
            
            // Create uniform buffer
            VkBufferCreateInfo buffer_create_info {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext = nullptr,
                .size = uniform_buffer_size,
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };

            VmaAllocationCreateInfo allocation_create_info {
                .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO,
            };
            VK_CHECK(vmaCreateBuffer(m_renderer->GetMemoryAllocator(), &buffer_create_info, &allocation_create_info, &m_frame_data[i].view_uniform_buffer, &m_frame_data[i].view_uniform_allocation, nullptr));
            
            // Initialize uniform buffer
            VkDescriptorBufferInfo buffer_info {
                .buffer = m_frame_data[i].view_uniform_buffer,
                .offset = 0,
                .range = uniform_buffer_size,
            };
            VkWriteDescriptorSet write_descriptor_set {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_frame_data[i].view_descriptor_set,
                .dstBinding = VIEW_BINDING,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_info
            };
            vkUpdateDescriptorSets(m_renderer->GetDevice(), 1, &write_descriptor_set, 0, nullptr);
        }
    }


    VkShaderModule vertex_shader_module = m_renderer->LoadShader(RESOURCE_PATH "shaders/triangle.vert.spv");
    VkShaderModule fragment_shader_module = m_renderer->LoadShader(RESOURCE_PATH "shaders/triangle.frag.spv");

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts {
        m_view_uniform_layout
    };
    std::vector<VkPushConstantRange> push_constant_ranges {};

    VkPipelineLayoutCreateInfo pipeline_layout_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size()),
        .pPushConstantRanges = push_constant_ranges.data(),
    };

    VK_CHECK(vkCreatePipelineLayout(m_renderer->GetDevice(), &pipeline_layout_create_info, nullptr, &m_triangle_pipeline_layout))

    m_triangle_pipeline = PipelineBuilder_Graphics(m_renderer->GetDevice(), m_triangle_pipeline_layout)
        .AddBinding(0, sizeof(Vertex))
        .AddAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, pos))
        .AddAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color))
    
        .AddColorAttachmentFormat(m_renderer->GetColorFormat())
        // .SetDepthAttachmentFormat(m_renderer->GetDepthStencilFormat())
        .AddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader_module)
        .AddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader_module)
        .Build();
        
    vkDestroyShaderModule(m_renderer->GetDevice(), vertex_shader_module, nullptr);
    vkDestroyShaderModule(m_renderer->GetDevice(), fragment_shader_module, nullptr);
}

void PongGame::DestroyPipelines() {
    vkDestroyPipeline(m_renderer->GetDevice(), m_triangle_pipeline, nullptr);
    vkDestroyPipelineLayout(m_renderer->GetDevice(), m_triangle_pipeline_layout, nullptr);

    const uint32_t SWAPCHAIN_COUNT = m_renderer->SwapChainImageCount();
    for (uint32_t i = 0; i < SWAPCHAIN_COUNT; ++i) {
        vmaDestroyBuffer(m_renderer->GetMemoryAllocator(), m_frame_data[i].view_uniform_buffer, m_frame_data[i].view_uniform_allocation);
        vkFreeDescriptorSets(m_renderer->GetDevice(), m_renderer->GetDescriptorPool(), 1, &m_frame_data[i].view_descriptor_set);
    }
    vkDestroyDescriptorSetLayout(m_renderer->GetDevice(), m_view_uniform_layout, nullptr);
}

void PongGame::UpdateUniforms() {
    uint32_t i = m_renderer->GetCurrentFrameIndex();

    // Map GPU memory onto CPU and edit it
    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(m_renderer->GetMemoryAllocator(), m_frame_data[i].view_uniform_allocation, &allocation_info);
    UniformData *color_uniform = static_cast<UniformData *>(allocation_info.pMappedData);

    color_uniform->color = m_current_color;
}

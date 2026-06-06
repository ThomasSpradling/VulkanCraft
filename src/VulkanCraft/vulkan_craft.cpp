#include "../Graphics/utils.h"

// #include "../backend/resource_manager/text_resource.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include "../Events/WindowEvents/key_events.h"
#include "../Events/WindowEvents/mouse_events.h"
#include "../Events/event_dispatcher.h"

#include "../Graphics/pipeline_builder.h"
#include <vulkan/vulkan_core.h>
#include "vulkan_craft.h"
#include <iostream>
#include <vector>

void VulkanCraft::Initialize() {
    std::cout << "Game initialized!" << std::endl;
    m_event_handler->AddListener(this);

    m_frame_data.resize(m_renderer->SwapChainImageCount());
    InitRenderTargets();
    InitPipelines();
    InitGeometry();

    // auto text_handle = m_resource_manager->Load<TextResource>("hello_world");

    // std::cout << text_handle.Get()->Contents() << std::endl;
}

void VulkanCraft::ShutDown() {
    std::cout << "Game shutting down!" << std::endl;
    m_event_handler->RemoveListener(this);

    DestroyGeometry();
    DestroyPipelines();
    DestroyRenderTargets();
}

void VulkanCraft::Update(float delta_time) {
    // auto *text = m_resource_manager->GetResource<TextResource>("hello_world");
    // std::cout << text->Contents() << std::endl;

    static float time = 0.0f;
    time += delta_time * 0.0005f;

    const auto extent = m_renderer->DrawExtent();

    float aspect =
        static_cast<float>(extent.width) /
        static_cast<float>(extent.height);

    glm::mat4 view = glm::translate(
        glm::mat4(1.0f),
        glm::vec3(0.0f, 0.0f, -5.0f)
    );

    glm::mat4 projection = glm::perspective(
        glm::radians(60.0f),
        aspect,
        0.1f,
        100.0f
    );
    projection[1][1] *= -1.0f;

    glm::vec3 spin_axis = glm::normalize(glm::vec3(1.0f, 1.0f, 0.5f));
    glm::mat4 model = glm::rotate(
        glm::mat4(1.0f),
        time * glm::radians(90.0f),
        glm::vec3(1.0f, 1.0f, 0.5f)
    );

    m_push_constants.view_projection = projection * view * model;
}

void VulkanCraft::Render(std::optional<Frame> frame, float delta_time) {
    uint32_t i = m_renderer->GetCurrentFrameIndex();

    if (!frame.has_value()) {
        m_frame_data.resize(m_renderer->SwapChainImageCount());
        // Window has been resized or modified
        DestroyRenderTargets();
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
    
        // Bind graphics and draw
        vkCmdBindPipeline(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_triangle_pipeline);
        vkCmdBindDescriptorSets(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_triangle_pipeline_layout, 0, 1, &m_frame_data[i].view_descriptor_set, 0, nullptr);
        vkCmdPushConstants(frame->cmd, m_triangle_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &m_push_constants);

        VkDeviceSize offsets[] = { 0 };

        for (auto &mesh : m_model->GetMeshes()) {
            vkCmdBindVertexBuffers(frame->cmd, 0, 1, &mesh->mesh_buffers.vertex_buffer, offsets);
            vkCmdBindIndexBuffer(frame->cmd, mesh->mesh_buffers.index_buffer, 0, VK_INDEX_TYPE_UINT32);
            
            for (auto &primitive : mesh->primitives) {
                vkCmdDrawIndexed(frame->cmd, primitive.index_count, 1, primitive.start_index, 0, 0);
            }
        }

        // vkCmdDraw(frame->cmd, 3, 1, 0, 0);
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
    m_model = std::make_shared<GLTFModel>(*m_renderer, RESOURCE_PATH "/models/Cube/Cube.gltf");
}

void VulkanCraft::DestroyGeometry() {
    // m_renderer->DestroyGPUMesh(m_mesh);
    m_model->CleanUp();
}

void VulkanCraft::InitPipelines() {
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
        
        for (uint32_t i = 0; i < m_frame_data.size(); ++i) {
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
    
    VkPushConstantRange push_constant_data {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstantData),
    };

    VkShaderModule vertex_shader_module = m_renderer->LoadShader(RESOURCE_PATH "/shaders/triangle.vert.spv");
    VkShaderModule fragment_shader_module = m_renderer->LoadShader(RESOURCE_PATH "/shaders/triangle.frag.spv");

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts {
        m_view_uniform_layout
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

    VK_CHECK(vkCreatePipelineLayout(m_renderer->GetDevice(), &pipeline_layout_create_info, nullptr, &m_triangle_pipeline_layout))

    m_triangle_pipeline = PipelineBuilder_Graphics(m_renderer->GetDevice(), m_triangle_pipeline_layout)
        .AddBinding(0, sizeof(MeshVertex))
        .AddAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, position))
        .AddAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, normal))
        .AddAttribute(2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(MeshVertex, color))
        .AddAttribute(3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(MeshVertex, uv))
    
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
    vkDestroyPipeline(m_renderer->GetDevice(), m_triangle_pipeline, nullptr);
    vkDestroyPipelineLayout(m_renderer->GetDevice(), m_triangle_pipeline_layout, nullptr);

    for (uint32_t i = 0; i < m_frame_data.size(); ++i) {
        vmaDestroyBuffer(m_renderer->GetMemoryAllocator(), m_frame_data[i].view_uniform_buffer, m_frame_data[i].view_uniform_allocation);
        vkFreeDescriptorSets(m_renderer->GetDevice(), m_renderer->GetDescriptorPool(), 1, &m_frame_data[i].view_descriptor_set);
    }
    vkDestroyDescriptorSetLayout(m_renderer->GetDevice(), m_view_uniform_layout, nullptr);
}

void VulkanCraft::UpdateUniforms() {
    uint32_t i = m_renderer->GetCurrentFrameIndex();

    // Map GPU memory onto CPU and edit it
    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(m_renderer->GetMemoryAllocator(), m_frame_data[i].view_uniform_allocation, &allocation_info);
    UniformData *color_uniform = static_cast<UniformData *>(allocation_info.pMappedData);

    color_uniform->color = m_current_color;
}

#pragma once
// #include "../game.h"
#include "../application.h"
#include "../Events/event_listener.h"
#include "../Graphics/mesh.h"
#include <glm/glm.hpp>
#include <vector>
#include "../Graphics/gltf_model.h"

struct UniformData {
    glm::vec3 color;
};

class VulkanCraft : public Game, public EventListener {
public:
    virtual void Initialize() override;
    virtual void ShutDown() override;
    virtual void Update(float delta_time) override;
    virtual void Render(std::optional<Frame> frame, float delta_time) override;
    virtual void OnEvent(const Event &event) override;
private:
    struct PerFrameData {
        VkDescriptorSet view_descriptor_set = VK_NULL_HANDLE;
        VkBuffer view_uniform_buffer = VK_NULL_HANDLE;
        VmaAllocation view_uniform_allocation = VK_NULL_HANDLE;

        VkImage depth_image;
        VkImageView depth_image_view;
        VmaAllocation depth_image_alloc;
    };

    struct PushConstantData {
        glm::mat4 view_projection;
        VkDeviceAddress vertex_buffer;
    };
private:
    float m_money = 0.0f;
    bool m_claimed_prize = false;
    glm::vec3 m_current_color = { 164.0f/256.0f, 30.0f/256.0f, 34.0f/256.0f };

    VkPipelineLayout m_triangle_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline m_triangle_pipeline = VK_NULL_HANDLE;

    // GPUMesh m_mesh;
    std::shared_ptr<GLTFModel> m_model;

    VkDescriptorSetLayout m_view_uniform_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_model_uniform_layout = VK_NULL_HANDLE;

    PushConstantData m_push_constants;

    std::unique_ptr<DescriptorAllocator> m_descriptor_allocator; 

    std::vector<PerFrameData> m_frame_data;
    UniformData m_uniform_data;
private:
    void InitRenderTargets();
    void DestroyRenderTargets();

    void InitGeometry();
    void DestroyGeometry();

    void InitPipelines();
    void DestroyPipelines();

    void UpdateUniforms();
};

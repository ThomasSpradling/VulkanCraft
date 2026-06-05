#pragma once
// #include "../game.h"
#include "../application.h"
#include "../events/event_listener.h"
#include <glm/glm.hpp>
#include <vector>

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

struct UniformData {
    glm::vec3 color;
};

class PongGame : public Game, public EventListener {
public:
    virtual void Initialize() override;
    virtual void ShutDown() override;
    virtual void Update(float delta_time) override;
    virtual void Render(const Frame &frame, float delta_time) override;
    virtual void OnEvent(const Event &event) override;
private:
    struct PerFrameData {
        VkDescriptorSet view_descriptor_set = VK_NULL_HANDLE;
        VkBuffer view_uniform_buffer = VK_NULL_HANDLE;
        VmaAllocation view_uniform_allocation = VK_NULL_HANDLE;
    };
private:
    float m_money = 0.0f;
    bool m_claimed_prize = false;
    glm::vec3 m_current_color = { 164.0f/256.0f, 30.0f/256.0f, 34.0f/256.0f };

    VkPipelineLayout m_triangle_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline m_triangle_pipeline = VK_NULL_HANDLE;

    VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
    VmaAllocation m_vertex_buffer_alloc = VK_NULL_HANDLE;

    VkBuffer m_index_buffer = VK_NULL_HANDLE;
    VmaAllocation m_index_buffer_alloc = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_view_uniform_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_model_uniform_layout = VK_NULL_HANDLE;

    std::vector<PerFrameData> m_frame_data;
    UniformData m_uniform_data;
private:
    void InitGeometry();
    void DestroyGeometry();

    void InitPipelines();
    void DestroyPipelines();

    void UpdateUniforms();
};

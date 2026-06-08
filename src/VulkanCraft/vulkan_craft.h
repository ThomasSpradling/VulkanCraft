#pragma once
// #include "../game.h"
#include "../application.h"
#include "../Events/event_listener.h"
#include "../Graphics/gpu_structs.h"
#include <glm/glm.hpp>
#include <vector>
#include "../Graphics/gltf_model.h"
#include "../Graphics/renderable.h"

struct SceneData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
    glm::vec4 ambient_color;
    glm::vec4 sunlight_direction;
    glm::vec4 sunlight_color;
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
        VkDescriptorSet global_descriptor_set = VK_NULL_HANDLE;
        VkBuffer global_uniform_buffer = VK_NULL_HANDLE;
        VmaAllocation global_uniform_buffer_alloc = VK_NULL_HANDLE;

        VkImage depth_image;
        VkImageView depth_image_view;
        VmaAllocation depth_image_alloc;
    };

    struct PushConstantData {
        glm::mat4 model;
        VkDeviceAddress vertex_buffer;
    };
private:
    // Temporary for input testing
    glm::vec3 m_current_color = { 164.0f/256.0f, 30.0f/256.0f, 34.0f/256.0f };

    std::shared_ptr<GLTFModel> m_model;

    VkDescriptorSetLayout m_global_layout = VK_NULL_HANDLE;
    GLTFCoreData m_gltf_common_data;

    PushConstantData m_push_constants;

    std::unique_ptr<DescriptorAllocator> m_descriptor_allocator; 

    std::vector<PerFrameData> m_frame_data;
    SceneData m_scene_data;

    DrawContext m_draw_context;
    // std::vector<RenderObject> m_render_objects;
private:
    void InitRenderTargets();
    void DestroyRenderTargets();

    void InitGeometry();
    void DestroyGeometry();

    void InitTextures();
    void DestroyTextures();

    void InitPipelines();
    void DestroyPipelines();

    void UpdateUniforms();
};

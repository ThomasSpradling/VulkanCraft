#pragma once

#include "Camera.h"
#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include "Platform/Graphics/DescriptorAllocator.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanDevice.h"
#include "Platform/Graphics/VulkanSwapChain.h"
#include "Platform/Window/Window.h"
#include "Platform/Graphics/ShaderCompiler.h"
#include <memory>
#include <optional>

struct MeshVertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

class Renderer : public NonCopyable, public NonMovable {
public:
    Renderer(const Window &window);
    ~Renderer();

    Camera &GetCamera() { return m_camera; }

    void EnableVSync();
    void DisableVSync();

    void DrawFrame();
private:
    struct FrameContext {
        std::unique_ptr<VulkanCommandPool> command_pool;
        std::unique_ptr<CommandBuffer> command_buffer;

        std::unique_ptr<VulkanFence> graphics_submit_fence;
        std::unique_ptr<VulkanSemaphore> image_available;

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        std::unique_ptr<VulkanBuffer> scene_uniform_buffer;
    };
    
    struct SwapChainContext {
        std::unique_ptr<VulkanSemaphore> render_finished;
        std::unique_ptr<VulkanImage> multisampled_image;
        std::unique_ptr<VulkanImage> draw_image;
        std::unique_ptr<VulkanImage> depth_buffer;
    };

    struct SceneUniformData {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec4 sun_direction; // w = power
        float ambient;
    };  

    struct PushConstantData {
        glm::mat4 model;
    };
private:
    static constexpr uint32_t MaxFramesInFlight = 2;
    const Window &m_window;

    std::unique_ptr<VulkanDevice> m_device = nullptr;
    std::unique_ptr<VulkanSwapChain> m_swapchain = nullptr;
    std::unique_ptr<ShaderCompiler> m_shader_compiler = nullptr;

    std::vector<FrameContext> m_frame_data;
    std::vector<SwapChainContext> m_swapchain_data;

    uint32_t m_frame_index = 0;

    bool m_enable_vsync = false;

    // App specific data
    std::unique_ptr<DescriptorAllocator> m_descriptor_allocator;
    VkDescriptorSetLayout m_scene_layout = VK_NULL_HANDLE;

    VkPipelineLayout m_triangle_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline m_triangle_pipeline = VK_NULL_HANDLE;

    std::optional<CompiledShader> m_triangle_shader;

    std::unique_ptr<VulkanBuffer> m_vertex_buffer;
    std::unique_ptr<VulkanBuffer> m_index_buffer;

    PushConstantData m_push_constant;

    Camera m_camera {};
private:
    void CreateObjects();
    void DestroyObjects();

    void CreateSwapChainObjects();
    void DestroySwapChainObjects();

    void CreateTrianglePipeline();
    void DestroyTrianglePipeline();

    void RecreateSwapChain();

    void UpdateSceneData();
    void RecordCommands(const FrameContext &frame);
};

#pragma once

// #include "Camera.h"
#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include "GarbageCollector.h"
#include "Platform/Graphics/VulkanDevice.h"
#include "Platform/Graphics/VulkanSwapChain.h"
#include "Platform/Window/Window.h"
#include "Platform/Graphics/ShaderCompiler.h"
#include "Platform/Graphics/VulkanPipeline.h"
#include <future>
#include <memory>
#include <optional>
#include <vector>

#include "Core/Handle.h"
#include "Renderer/BindlessDescriptorTable.h"
#include "World/World.h"

[[maybe_unused]] constexpr uint32_t MaxPointLights = 100;

enum class AntiAliasing : uint8_t {
    Disabled,
    MSAAx2,
    MSAAx4,
    MSAAx8,
};

struct RendererSettings {
    AntiAliasing anti_aliasing = AntiAliasing::Disabled;
    bool vsync = false;
};

struct RendererCapabilities {
    bool hardware_ray_tracing = false;
    bool mesh_shaders = false;

    std::uint32_t maximum_msaa_samples = 1;
};

struct EnvironmentSettings {
    glm::vec4 ambient_color = glm::vec4(1.0f);
    float ambient_intensity = 0.2f;

    TextureHandle environment_map = TextureHandle::Invalid();
    float environment_rotation = 0.0f;
};

struct SceneRenderOptions {
    bool render_debug_geometry = true;
};

class Renderer : public NonCopyable, public NonMovable {
public:
    Renderer(const VulkanDevice &device, const Window &window);
    ~Renderer();

    void Initialize();
    void Configure(const RendererSettings &settings);
    RendererSettings Settings() const;
    
    RendererCapabilities Capabilities() const;

    void SetClearColor(const glm::vec4 &color);
    glm::vec4 ClearColor() const;

    void SetEnvironment(const EnvironmentSettings &settings);
    EnvironmentSettings Environment() const;

    void EnableVSync();
    void DisableVSync();

    void RenderScene(World &world, Entity camera, const SceneRenderOptions &options = {});
    void AttachAssetManager(AssetManager &manager) { m_asset_manager = &manager; }
    const VulkanDevice &Device() const { return m_device; }

    BindlessDescriptorTable &BindlessTable() { return *m_bindless_table; }
    GarbageCollector &GPUGargbageCollector() { return *m_garbage_collector; }
private:
    struct FrameContext {
        std::unique_ptr<VulkanCommandPool> command_pool;
        std::unique_ptr<CommandBuffer> command_buffer;

        std::unique_ptr<VulkanFence> graphics_submit_fence;
        std::unique_ptr<VulkanSemaphore> image_available;
        BufferHandle scene_uniform_buffer = BufferHandle::Invalid();

        // Lights
        BufferHandle light_data = BufferHandle::Invalid();
        BufferHandle point_lights = BufferHandle::Invalid();
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
        glm::vec4 eye_position;
        float ambient;
        VkDeviceAddress light_data;
    };  

    struct PushConstantData {
        glm::mat4 model;
        VkDeviceAddress vertex_buffer;
        VkDeviceAddress scene_data_buffer;
        VkDeviceAddress material_buffer;
        uint32_t material_id = 0;
    };

    struct SpecializationConstantData {
        VkBool32 is_wireframe = false;
    };
private:
    static constexpr uint32_t MaxFramesInFlight = 3;
    const VulkanDevice &m_device;
    const Window &m_window;

    AssetManager *m_asset_manager = nullptr;
    
    // Settings
    bool m_enable_vsync = false;

    // GPU Context
    std::unique_ptr<VulkanSwapChain> m_swapchain = nullptr;
    std::unique_ptr<ShaderCompiler> m_shader_compiler = nullptr;

    std::vector<SwapChainContext> m_swapchain_data;
    
    // Frame Data
    uint32_t m_frame_index = 0;
    std::vector<FrameContext> m_frame_data;

    // Rendering Data
    std::unique_ptr<VulkanPipeline> m_triangle_pipeline;
    std::unique_ptr<VulkanPipeline> m_debug_pipeline;
    std::unique_ptr<VulkanPipeline> m_wireframe_pipeline;

    std::optional<CompiledShader> m_triangle_shader;

    PushConstantData m_push_constant;
    SpecializationConstantData m_specialization_constant;

    std::unique_ptr<BindlessDescriptorTable> m_bindless_table;
    std::unique_ptr<GarbageCollector> m_garbage_collector;
private:
    // void DrawGLTF(GLTFModel &model, const CommandBuffer &cmd);

    void CreateObjects();
    void DestroyObjects();

    void CreateSwapChainObjects();
    void DestroySwapChainObjects();

    void CreateTrianglePipeline();
    void DestroyTrianglePipeline();

    void RecreateSwapChain();

    void UpdateSceneData(Entity camera);
    void RecordCommands(const FrameContext &frame, World &world, Entity camera, const SceneRenderOptions &options);

    void CreateLights();
};

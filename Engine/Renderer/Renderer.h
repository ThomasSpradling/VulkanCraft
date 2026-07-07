#pragma once

#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include "Platform/Graphics/DescriptorAllocator.h"
#include "Platform/Graphics/VulkanDevice.h"
#include "Platform/Graphics/VulkanSwapChain.h"
#include "Platform/Window/Window.h"
#include "Platform/Graphics/ShaderCompiler.h"
#include <memory>

class Renderer : public NonCopyable, public NonMovable {
public:
    Renderer(const Window &window);
    ~Renderer();

    void DrawFrame();
private:
    struct FrameContext {
        std::unique_ptr<VulkanCommandPool> command_pool;
        std::unique_ptr<CommandBuffer> command_buffer;

        std::unique_ptr<VulkanFence> frame_complete_fence;
        std::unique_ptr<VulkanSemaphore> image_available;
    };
    
    struct SwapChainContext {
        std::unique_ptr<VulkanSemaphore> render_finished;
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

    VkPipelineLayout m_triangle_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline m_triangle_pipeline = VK_NULL_HANDLE;

    std::optional<CompiledShader> m_triangle_shader;

    bool m_enable_vsync = false;
private:
    void CreateObjects();
    void DestroyObjects();

    void CreateSwapChainObjects();
    void DestroySwapChainObjects();

    void CreateTrianglePipeline();
    void DestroyTrianglePipeline();

    void RecreateSwapChain();

    void RecordCommands(const FrameContext &frame);
};

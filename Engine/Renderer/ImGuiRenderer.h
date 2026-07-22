#pragma once

#include "AssetManager/GPUStructs.h"
#include "Core/Handle.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Platform/Graphics/VulkanPipeline.h"
#include <imgui.h>

class Renderer;
class ImGuiRenderer {
public:
    ImGuiRenderer(Renderer &renderer);
    ~ImGuiRenderer();

    void BeginFrame();
    void EndFrame(const CommandBuffer &cmd, const ImageAttachment &image_attachment);
private:
    Renderer &m_renderer;

    struct ImGuiPushConstant {
        glm::vec4 viewport_planes; // left, right, top, bottom
        VkDeviceAddress vertex_buffer;
        TextureId texture_id;
        SamplerId sampler_id;
    };

    struct FrameData {
        BufferHandle vertex_buffer;
        BufferHandle index_buffer;
        uint32_t vertex_count = 0;
        uint32_t index_count = 0;
    };

    std::vector<FrameData> m_frame_data;
    CompiledShader m_shader;

    std::unique_ptr<VulkanPipeline> m_pipeline;

    SamplerId m_sampler = 0;

    std::unordered_map<ImTextureData *, TextureHandle> m_textures;
private:
    void UpdateFont(float font_size_pixels);
};

#pragma once

#include "Common.h"
#include "Core/Handle.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Platform/Graphics/ShaderCompiler.h"
#include "Platform/Graphics/VulkanPipeline.h"
#include <glm/glm.hpp>
#include <optional>
#include <unordered_map>

struct DebugVertex {
    glm::vec4 position;
    glm::vec4 color;
};

class Renderer;
class DebugCanvas {
public:
    DebugCanvas(Renderer &renderer);

    void Clear();
    void SetColor(glm::vec4 color);

    void DrawLine(glm::vec3 from_position, glm::vec3 to_position, std::optional<glm::vec4> color = std::nullopt);
    void DrawSphere(glm::vec3 center_position, float radius, std::optional<glm::vec4> color = std::nullopt);
    void DrawCircle(glm::vec3 center_position, float radius = 1.0f, glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f), std::optional<glm::vec4> color = std::nullopt);
    void DrawAxes(const glm::mat4 &transform, float size);
    void DrawTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, std::optional<glm::vec4> color = std::nullopt);
    void DrawAABB(glm::vec3 min_coords, glm::vec3 max_coords, std::optional<glm::vec4> color = std::nullopt);
    void DrawOBB(const glm::mat4 &center_transform, glm::vec3 scale, std::optional<glm::vec4> color = std::nullopt);
    void DrawFrustrum(const glm::mat4 &view, const glm::mat4 &projection, std::optional<glm::vec4> color = std::nullopt);

    void Render(const CommandBuffer &cmd, const glm::mat4 &transform, const ImageAttachment &image_attachment, const ImageAttachment &depth_attachment);
private:
    struct FrameData {
        BufferHandle vertex_buffer;
        uint32_t current_buffer_size;
    };

    struct PushConstantData {
        glm::mat4 transform;
        VkDeviceAddress vertex_buffer;
    };
    static_assert(sizeof(PushConstantData) <= MaxPushConstantSize);

    struct VertexEntry {
        uint32_t vertex_offset;
    };
private:
    Renderer &m_renderer;

    std::vector<FrameData> m_frame_data;

    std::vector<DebugVertex> m_vertices;

    CompiledShader m_debug_shader;
    std::unique_ptr<VulkanPipeline> m_debug_pipeline;

    // Immediate data
    glm::vec4 m_color = glm::vec4(1.0f);
};

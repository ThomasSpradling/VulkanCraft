#include "DebugRenderer.h"
#include "AssetManager/AssetManager.h"
#include "AssetManager/Buffer.h"
#include "AssetManager/Common.h"
#include "Platform/Graphics/ShaderCompiler.h"
#include "Platform/Graphics/VulkanPipeline.h"
#include "Renderer.h"
#include <format>
#include <stdexcept>

DebugCanvas::DebugCanvas(Renderer &renderer)
    : m_renderer(renderer)
{
    m_frame_data.resize(renderer.FramesInFlight());

    std::optional<CompiledShader> shader = renderer.GetShaderCompiler().Compile("Debug");
    if (!shader)
        throw std::runtime_error("Cannot compile debug shader!");

    m_debug_shader = std::move(*shader);

    m_debug_pipeline = VulkanPipeline::GraphicsBuilder(renderer.Device(), m_renderer.GlobalPipelineLayout())
        .VertexShader(m_debug_shader, "main_vert")
        .FragmentShader(m_debug_shader, "main_frag")
        // .AddDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH)
        .AddColorAttachment(ColorAttachment {
            .format = renderer.Device().GetLinearColorFormat(),
            .blending_mode = BlendMode::Alpha,
        })
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_LINE)
        .SetDepthAttachmentFormat(renderer.Device().GetDepthOnlyFormat())
        .EnableDepthTest()
        .Build();
}

void DebugCanvas::Clear() {
    m_vertices.clear();
}

void DebugCanvas::SetColor(glm::vec4 color) {
    m_color = color;
}

void DebugCanvas::DrawLine(glm::vec3 from_position, glm::vec3 to_position, std::optional<glm::vec4> color) {
    glm::vec4 draw_color = color ? *color : m_color;

    m_vertices.push_back({
        .position = glm::vec4(from_position, 1.0f),
        .color = draw_color,
    });

    m_vertices.push_back({
        .position = glm::vec4(to_position, 1.0f),
        .color = draw_color,
    });
}

void DebugCanvas::DrawSphere(glm::vec3 center_position, float radius, std::optional<glm::vec4> color) {
    ENGINE_PROFILER_FUNCTION();

    constexpr int segment_count = 24;
    constexpr int ring_count = 12;

    if (radius <= 0.0f)
        return;

    static_assert(segment_count >= 3);
    static_assert(ring_count >= 2);

    const auto sphere_point = [&](int ring, int segment) {
        const float latitude = -glm::half_pi<float>() + glm::pi<float>() * static_cast<float>(ring) / static_cast<float>(ring_count);
        const float longitude = glm::two_pi<float>() * static_cast<float>(segment) / static_cast<float>(segment_count);
        const float latitude_radius = std::cos(latitude);

        return center_position + radius * glm::vec3(latitude_radius * std::cos(longitude), std::sin(latitude), latitude_radius * std::sin(longitude));
    };

    for (int ring = 0; ring <= ring_count; ++ring) {
        for (int segment = 0; segment < segment_count; ++segment) {
            const glm::vec3 position = sphere_point(ring, segment);

            if (ring > 0 && ring < ring_count)
                DrawLine(position, sphere_point(ring, segment + 1), color);

            if (ring < ring_count)
                DrawLine(position, sphere_point(ring + 1, segment), color);
        }
    }
}

void DebugCanvas::DrawCircle(glm::vec3 center_position, float radius, glm::vec3 normal, std::optional<glm::vec4> color) {
    constexpr int segment_count = 64;
    constexpr float epsilon = 1.0e-8f;

    const float normal_length_squared = glm::dot(normal, normal);
    if (radius <= 0.0f || normal_length_squared <= epsilon)
        return;

    normal /= std::sqrt(normal_length_squared);

    const glm::vec3 reference = std::abs(normal.z) < 0.999f
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);

    const glm::vec3 tangent = glm::normalize(glm::cross(reference, normal));
    const glm::vec3 bitangent = glm::cross(normal, tangent);
    glm::vec3 previous_position = center_position + tangent * radius;

    for (int i = 1; i <= segment_count; ++i) {
        const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(segment_count);
        const glm::vec3 position = center_position + (tangent * std::cos(angle) + bitangent * std::sin(angle)) * radius;

        DrawLine(previous_position, position, color);
        previous_position = position;
    }
}

void DebugCanvas::DrawAxes(const glm::mat4 &transform, float size) {
    if (size <= 0.0f)
        return;

    const glm::vec4 previous_color = m_color;

    const auto transform_point = [&transform](glm::vec3 position) {
        return glm::vec3(transform * glm::vec4(position, 1.0f));
    };

    const auto draw_axis = [&](glm::vec3 axis, glm::vec3 tangent, glm::vec3 bitangent, glm::vec4 color) {
        const glm::vec3 origin(0.0f);
        const glm::vec3 tip = axis * size;

        SetColor(color);
        DrawLine(transform_point(origin), transform_point(tip), color);
    };

    draw_axis(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    draw_axis(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    draw_axis(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    SetColor(previous_color);
}

void DebugCanvas::DrawTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, std::optional<glm::vec4> color) {
    DrawLine(v0, v1, color);
    DrawLine(v1, v2, color);
    DrawLine(v2, v0, color);
}

namespace {
    constexpr std::uint8_t BoxEdges[12][2] = {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
    };
}

void DebugCanvas::DrawAABB(glm::vec3 min_coords, glm::vec3 max_coords, std::optional<glm::vec4> color) {
    const glm::vec3 minimum = glm::min(min_coords, max_coords);
    const glm::vec3 maximum = glm::max(min_coords, max_coords);

    const std::array<glm::vec3, 8> corners = {
        glm::vec3(minimum.x, minimum.y, minimum.z),
        glm::vec3(maximum.x, minimum.y, minimum.z),
        glm::vec3(maximum.x, maximum.y, minimum.z),
        glm::vec3(minimum.x, maximum.y, minimum.z),
        glm::vec3(minimum.x, minimum.y, maximum.z),
        glm::vec3(maximum.x, minimum.y, maximum.z),
        glm::vec3(maximum.x, maximum.y, maximum.z),
        glm::vec3(minimum.x, maximum.y, maximum.z),
    };

    for (const auto &edge : BoxEdges)
        DrawLine(corners[edge[0]], corners[edge[1]], color);
}

void DebugCanvas::DrawOBB(const glm::mat4 &center_transform, glm::vec3 scale, std::optional<glm::vec4> color) {
    const glm::vec3 half_extents = glm::abs(scale) * 0.5f;

    const std::array<glm::vec3, 8> local_corners = {
        glm::vec3(-half_extents.x, -half_extents.y, -half_extents.z),
        glm::vec3( half_extents.x, -half_extents.y, -half_extents.z),
        glm::vec3( half_extents.x,  half_extents.y, -half_extents.z),
        glm::vec3(-half_extents.x,  half_extents.y, -half_extents.z),
        glm::vec3(-half_extents.x, -half_extents.y,  half_extents.z),
        glm::vec3( half_extents.x, -half_extents.y,  half_extents.z),
        glm::vec3( half_extents.x,  half_extents.y,  half_extents.z),
        glm::vec3(-half_extents.x,  half_extents.y,  half_extents.z),
    };

    std::array<glm::vec3, 8> world_corners;
    for (std::size_t i = 0; i < local_corners.size(); ++i)
        world_corners[i] = glm::vec3(center_transform * glm::vec4(local_corners[i], 1.0f));

    for (const auto &edge : BoxEdges)
        DrawLine(world_corners[edge[0]], world_corners[edge[1]], color);
}

void DebugCanvas::DrawFrustrum(const glm::mat4 &view, const glm::mat4 &projection, std::optional<glm::vec4> color) {
    constexpr float near_depth = 0.0f;

    constexpr float far_depth = 1.0f;
    constexpr float epsilon = 1.0e-8f;

    const std::array<glm::vec3, 8> ndc_corners = {
        glm::vec3(-1.0f, -1.0f, near_depth),
        glm::vec3( 1.0f, -1.0f, near_depth),
        glm::vec3( 1.0f,  1.0f, near_depth),
        glm::vec3(-1.0f,  1.0f, near_depth),
        glm::vec3(-1.0f, -1.0f, far_depth),
        glm::vec3( 1.0f, -1.0f, far_depth),
        glm::vec3( 1.0f,  1.0f, far_depth),
        glm::vec3(-1.0f,  1.0f, far_depth),
    };

    const glm::mat4 inverse_view_projection = glm::inverse(projection * view);
    std::array<glm::vec3, 8> world_corners;

    for (std::size_t i = 0; i < ndc_corners.size(); ++i) {
        const glm::vec4 homogeneous =
            inverse_view_projection * glm::vec4(ndc_corners[i], 1.0f);

        if (std::abs(homogeneous.w) <= epsilon)
            return;

        world_corners[i] = glm::vec3(homogeneous) / homogeneous.w;
    }

    for (const auto &edge : BoxEdges)
        DrawLine(world_corners[edge[0]], world_corners[edge[1]], color);
}

void DebugCanvas::Render(const CommandBuffer &cmd, const glm::mat4 &transform, const ImageAttachment &image_attachment, const ImageAttachment &depth_attachment) {
    FrameData &frame = m_frame_data[m_renderer.FrameIndex()];

    const auto required_size = static_cast<uint32_t>(m_vertices.size() * sizeof(DebugVertex));
    AssetManager &assets = m_renderer.GetAssetManager();
    if (frame.current_buffer_size < required_size) {
        // TODO: Grow in amortized fashion by doubling size each time
        if (frame.vertex_buffer)
            assets.DestroyBuffer(frame.vertex_buffer);

        frame.vertex_buffer = assets.CreateBuffer(GPUBufferData {
            .usage = BufferUsageBits::Storage,
            .storage_type = StorageType::HostVisible,
            .size = required_size,
            .data = m_vertices.data(),
            .debug_name = std::format("Debug Line Buffer [{}]", m_renderer.FrameIndex()),
        });

        frame.current_buffer_size = required_size;
    } else {
        assets.GetBuffer(frame.vertex_buffer).Upload(m_vertices);
    }

    const VulkanBuffer &vertex_buffer = assets.GetBuffer(frame.vertex_buffer);

    cmd.BeginRendering({ image_attachment, depth_attachment });
        // cmd.BindDescriptorSet(0, *m_debug_pipeline, m_renderer.BindlessTable().GlobalDescriptorSet());
        cmd.BindPipeline(*m_debug_pipeline);
        cmd.PushConstants(m_debug_pipeline->Layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, PushConstantData {
            .transform = transform,
            .vertex_buffer = vertex_buffer.DeviceAddress(),
        });
        cmd.Draw(static_cast<uint32_t>(m_vertices.size()));
    cmd.EndRendering();
}

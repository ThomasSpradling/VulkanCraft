#include "ImGuiRenderer.h"
#include "AssetManager/Buffer.h"
#include "AssetManager/Texture.h"
#include "Core/errors.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Renderer.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <iostream>

ImGuiRenderer::ImGuiRenderer(Renderer &renderer)
    : m_renderer(renderer)
{
    m_frame_data.resize(renderer.FramesInFlight());

    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui_ImplGlfw_InitForOther(renderer.GetWindow().GetHandle(), true);

    std::optional<CompiledShader> shader = renderer.GetShaderCompiler().Compile("ImGui");
    ENGINE_ASSERT(shader, "Cannot compile ImGui shader!");

    m_shader = std::move(*shader);
    auto sampler_handle = renderer.GetAssetManager().CreateSampler(TextureSamplerData {
        .min_filter = SamplerFilter::Linear,
        .mag_filter = SamplerFilter::Linear,
        .wrap_u = TextureWrapMode::ClampToEdge,
        .wrap_v = TextureWrapMode::ClampToEdge,
    });

    m_sampler = renderer.GetAssetManager().GetSampler(sampler_handle).first;

    m_pipeline = VulkanPipeline::GraphicsBuilder(m_renderer.Device(), m_renderer.GlobalPipelineLayout())
        .VertexShader(m_shader, "main_vert")
        .FragmentShader(m_shader, "main_frag")
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)

        .AddColorAttachment({
            .format = m_renderer.Device().GetLinearColorFormat(),
            .blending_mode = BlendMode::Alpha,
        })
        // .SetDepthAttachmentFormat(m_renderer.Device().GetDepthOnlyFormat())
        .Build();
}

ImGuiRenderer::~ImGuiRenderer() {
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiRenderer::UpdateFont(float font_size_pixels) {
    ImGuiIO &io = ImGui::GetIO();

    ImFontConfig config = ImFontConfig();
    config.RasterizerMultiply = 1.5f;
    config.SizePixels = ceilf(font_size_pixels);
    config.PixelSnapH = true;
    config.OversampleH = 4;
    config.OversampleV = 4;

    ImFont *font = io.Fonts->AddFontDefault(&config);
    io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
    io.FontDefault = font;
}

void ImGuiRenderer::BeginFrame() {
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiRenderer::EndFrame(const CommandBuffer &cmd, const ImageAttachment &image_attachment) {
    static_assert(sizeof(ImDrawIdx) == 2);

    ImGui::Render();

    ImDrawData *draw_data = ImGui::GetDrawData();
    const float width = static_cast<float>(image_attachment.image.Extent().width);
    const float height = static_cast<float>(image_attachment.image.Extent().height);
    if (width <= 0 || height <= 0 || draw_data->CmdListsCount == 0)
        return;

    if (draw_data->Textures) {
        for (ImTextureData *tex : *draw_data->Textures) {
            switch (tex->Status) {
                case ImTextureStatus_OK:
                    continue;
                case ImTextureStatus_Destroyed:
                    continue;
                case ImTextureStatus_WantCreate: {
                    ENGINE_ASSERT(tex->TexID == ImTextureID_Invalid && !tex->BackendUserData, "Invalid ImGui Texture!");
                    ENGINE_ASSERT(tex->Format == ImTextureFormat_RGBA32, "Invalid ImGui Texture!");
                    ENGINE_ASSERT(tex->BytesPerPixel == 4, "Invalid ImGui Texture!");
                    
                    std::vector<std::byte> pixels {};
                    auto image_size = static_cast<uint32_t>(tex->Width * tex->Height * 4);
                    pixels.resize(image_size);
                    for (uint32_t i = 0; i < image_size; ++i) {
                        pixels[i] = static_cast<std::byte>(tex->Pixels[i]);
                    }

                    TextureHandle texture = m_renderer.GetAssetManager().CreateTexture(Texture {
                        .extent = glm::uvec3(tex->Width, tex->Height, 1),
                        .format = TextureFormat::RGBA8,
                        .pixels = std::move(pixels),
                    });

                    m_textures.emplace(tex, texture);

                    const TextureId texture_id = m_renderer.GetAssetManager().GetTexture(texture).first;
                    std::cout << "Creating new ImGUi texture: " << texture_id << "\n";
                    tex->SetTexID(texture_id);
                    tex->SetStatus(ImTextureStatus_OK);
                    continue;
                }
                case ImTextureStatus_WantUpdates: {
                    ENGINE_ASSERT(tex->Format == ImTextureFormat_RGBA32, "Invalid texture.");
                    ENGINE_ASSERT(tex->BytesPerPixel == 4, "Invalid texture.");

                    auto texture_it = m_textures.find(tex);
                    ENGINE_ASSERT(texture_it != m_textures.end(), "Could not find ImGui texture.");

                    const auto upload_width = static_cast<uint32_t>(tex->UpdateRect.w);
                    const auto upload_height = static_cast<uint32_t>(tex->UpdateRect.h);
                    const uint32_t row_size = upload_width * tex->BytesPerPixel;

                    std::vector<std::byte> pixels(static_cast<size_t>(row_size) * upload_height);

                    for (uint32_t y = 0; y < upload_height; ++y) {
                        std::memcpy(pixels.data() + static_cast<size_t>(y) * row_size,
                            tex->GetPixelsAt(tex->UpdateRect.x, tex->UpdateRect.y + static_cast<int>(y)), row_size);
                    }

                    VulkanImage &texture = m_renderer.GetAssetManager().GetTexture(texture_it->second).second;

                    cmd.ImageMemoryBarrier(texture)
                        .SourceAccess(VK_ACCESS_2_SHADER_SAMPLED_READ_BIT)
                        .SourceStage(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT)
                        .DestAccess(VK_ACCESS_2_TRANSFER_WRITE_BIT)
                        .DestStage(VK_PIPELINE_STAGE_2_COPY_BIT)
                        .TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                        .Execute();
                    texture.Upload(cmd, TextureRange {
                        .offset = glm::ivec3(tex->UpdateRect.x, tex->UpdateRect.y, 0),
                        .dimensions = glm::uvec3(upload_width, upload_height, 1),
                    }, pixels.data());
                    cmd.ImageMemoryBarrier(texture)
                        .SourceAccess(VK_ACCESS_2_TRANSFER_WRITE_BIT)
                        .SourceStage(VK_PIPELINE_STAGE_2_COPY_BIT)
                        .DestAccess(VK_ACCESS_2_SHADER_SAMPLED_READ_BIT)
                        .DestStage(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT)
                        .TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        .Execute();

                    tex->SetStatus(ImTextureStatus_OK);
                    continue;
                }
                case ImTextureStatus_WantDestroy: {
                    auto texture_it = m_textures.find(tex);
                    if (texture_it != m_textures.end()) {
                        m_renderer.GetAssetManager().DestroyTexture(texture_it->second);
                        m_textures.erase(texture_it);
                    }

                    tex->SetTexID(ImTextureID_Invalid);
                    tex->SetStatus(ImTextureStatus_Destroyed);

                    continue;
                }
            }
        }
    }

    // cmd.BindDescriptorSet(0, *m_pipeline, m_renderer.BindlessTable().GlobalDescriptorSet());
    
    cmd.SetViewport(glm::vec2(0), glm::vec2(width, height));

    const ImVec2 clip_off = draw_data->DisplayPos;
    const ImVec2 clip_scale {
        width / draw_data->DisplaySize.x,
        height / draw_data->DisplaySize.y,
    };

    const float left = draw_data->DisplayPos.x;
    const float right = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    const float top = draw_data->DisplayPos.y;
    const float bottom = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    uint32_t frame_index = m_renderer.FrameIndex();
    FrameData &frame = m_frame_data[frame_index];
    if (static_cast<int>(frame.index_count) < draw_data->TotalIdxCount) {
        if (frame.index_buffer)
            m_renderer.GetAssetManager().DestroyBuffer(frame.index_buffer);

        frame.index_buffer = m_renderer.GetAssetManager().CreateBuffer(GPUBufferData {
            .usage = BufferUsageBits::Index,
            .storage_type = StorageType::HostVisible,
            .size = draw_data->TotalIdxCount * sizeof(ImDrawIdx),
            .debug_name = std::format("ImGui Index Buffer [{}]", frame_index),
        });
        frame.index_count = draw_data->TotalIdxCount;
    }

    if (static_cast<int>(frame.vertex_count) < draw_data->TotalVtxCount) {
        if (frame.vertex_buffer)
            m_renderer.GetAssetManager().DestroyBuffer(frame.vertex_buffer);

        frame.vertex_buffer = m_renderer.GetAssetManager().CreateBuffer(GPUBufferData {
            .usage = BufferUsageBits::Storage,
            .storage_type = StorageType::HostVisible,
            .size = draw_data->TotalVtxCount * sizeof(ImDrawVert),
            .debug_name = std::format("ImGui Vertex Buffer [{}]", frame_index),
        });
        frame.vertex_count = draw_data->TotalVtxCount;
    }

    //// Upload Data ////
    VulkanBuffer &index_buffer = m_renderer.GetAssetManager().GetBuffer(frame.index_buffer);
    VulkanBuffer &vertex_buffer = m_renderer.GetAssetManager().GetBuffer(frame.vertex_buffer);

    {
        auto *mapped_vertex_buffer = vertex_buffer.Mapped<ImDrawVert>();
        auto *mapped_index_buffer = index_buffer.Mapped<ImDrawIdx>();

        for (const ImDrawList *cmd_list : draw_data->CmdLists) {
            std::memcpy(mapped_vertex_buffer, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            std::memcpy(mapped_index_buffer, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));

            mapped_vertex_buffer += cmd_list->VtxBuffer.Size;
            mapped_index_buffer += cmd_list->IdxBuffer.Size;
        }

        vertex_buffer.FlushMappedMemory(0, draw_data->TotalVtxCount * sizeof(ImDrawVert));
        index_buffer.FlushMappedMemory(0, draw_data->TotalIdxCount * sizeof(ImDrawIdx));
    }
    
    uint32_t index_offset = 0;
    uint32_t vertex_offset = 0;

    cmd.BindIndexBuffer(index_buffer.Buffer(), 0, VK_INDEX_TYPE_UINT16);
    cmd.BindPipeline(*m_pipeline);

    cmd.BeginRendering({ image_attachment });
    
    for (const ImDrawList *cmd_list : draw_data->CmdLists) {
        for (int i = 0; i < cmd_list->CmdBuffer.Size; ++i) {
            const ImDrawCmd &draw_command = cmd_list->CmdBuffer[i];

            if (draw_command.UserCallback) {
                if (draw_command.UserCallback == ImDrawCallback_ResetRenderState) {
                    cmd.SetViewport(glm::vec2(0), glm::vec2(width, height));
                    // cmd.BindDescriptorSet(0, *m_pipeline, m_renderer.BindlessTable().GlobalDescriptorSet());
                    cmd.BindIndexBuffer(index_buffer.Buffer(), 0, VK_INDEX_TYPE_UINT16);
                    cmd.BindPipeline(*m_pipeline);
                } else {
                    draw_command.UserCallback(cmd_list, &draw_command);
                }
                continue;
            }

            float clip_min_x = (draw_command.ClipRect.x - clip_off.x) * clip_scale.x;
            float clip_min_y = (draw_command.ClipRect.y - clip_off.y) * clip_scale.y;
            float clip_max_x = (draw_command.ClipRect.z - clip_off.x) * clip_scale.x;
            float clip_max_y = (draw_command.ClipRect.w - clip_off.y) * clip_scale.y;

            clip_min_x = std::clamp(clip_min_x, 0.0f, width);
            clip_min_y = std::clamp(clip_min_y, 0.0f, height);
            clip_max_x = std::clamp(clip_max_x, 0.0f, width);
            clip_max_y = std::clamp(clip_max_y, 0.0f, height);

            if (clip_max_x <= clip_min_x || clip_max_y <= clip_min_y)
                continue;

            const auto scissor_x = static_cast<int32_t>(std::floor(clip_min_x));
            const auto scissor_y = static_cast<int32_t>(std::floor(clip_min_y));
            const auto scissor_width = static_cast<uint32_t>(std::ceil(clip_max_x) - static_cast<float>(scissor_x));
            const auto scissor_height = static_cast<uint32_t>(std::ceil(clip_max_y) - static_cast<float>(scissor_y));

            const VkRect2D scissor {
                .offset = { scissor_x, scissor_y },
                .extent = { scissor_width, scissor_height },
            };

            vkCmdSetScissor(cmd.Handle(), 0, 1, &scissor);
            const uint64_t command_vertex_offset = static_cast<uint64_t>(vertex_offset) + static_cast<uint64_t>(draw_command.VtxOffset);

            ImGuiPushConstant push_constant {
                .viewport_planes = { left, right, top, bottom },
                .vertex_buffer = vertex_buffer.DeviceAddress() + command_vertex_offset * sizeof(ImDrawVert),
                .texture_id = static_cast<TextureId>(draw_command.GetTexID()),
                .sampler_id = m_sampler,
            };

            cmd.PushConstants(m_pipeline->Layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, push_constant);
            cmd.SetScissors(glm::ivec2(scissor_x, scissor_y), glm::uvec2(scissor_width, scissor_height));
            cmd.DrawIndexed(draw_command.ElemCount, 1, index_offset + draw_command.IdxOffset);
        }
        index_offset += cmd_list->IdxBuffer.Size;
        vertex_offset += cmd_list->VtxBuffer.Size;
    }

    cmd.EndRendering();
}

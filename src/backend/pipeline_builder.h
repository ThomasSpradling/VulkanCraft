#pragma once

#include <volk.h>
#include <vector>

#include "../vulkan_renderer.h"
    
class PipelineBuilder_Graphics {
public:
    PipelineBuilder_Graphics(VkDevice renderer, VkPipelineLayout layout = VK_NULL_HANDLE);
    
    PipelineBuilder_Graphics &AddBinding(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX);
    PipelineBuilder_Graphics &AddAttribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset = 0);

    PipelineBuilder_Graphics &FromBase(VkPipeline base_pipeline);

    PipelineBuilder_Graphics &AddShaderStage(VkShaderStageFlagBits shader_stage, VkShaderModule shader_module);
    PipelineBuilder_Graphics &AddTesselationStage(VkShaderModule shader_control_module, VkShaderModule shader_eval_module, uint32_t patch_control_points);

    PipelineBuilder_Graphics &SetTopology(VkPrimitiveTopology topology);
    PipelineBuilder_Graphics &EnablePrimitiveRestart(bool enable_primitive_restart);

    PipelineBuilder_Graphics &EnableCulling(VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT);
    PipelineBuilder_Graphics &SetFrontFace(VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE);
    PipelineBuilder_Graphics &DisableCulling();

    PipelineBuilder_Graphics &SetPolygonMode(VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL);
    PipelineBuilder_Graphics &SetLineWidth(float width);

    PipelineBuilder_Graphics &EnableDepthTest(VkCompareOp compare_op = VK_COMPARE_OP_LESS);
    PipelineBuilder_Graphics &DisableDepthWrite();
    PipelineBuilder_Graphics &EnableStencilTest();

    PipelineBuilder_Graphics &DisableMSAA();
    PipelineBuilder_Graphics &Enable4xMSAA(float min_sample_shading = 0.2f);
    PipelineBuilder_Graphics &Enable8xMSAA(float min_sample_shading = 1.0f);
    PipelineBuilder_Graphics &EnableAlphaToCoverageMSAA();

    /** @brief Enabled opaque blending state by disabling blending. */
    PipelineBuilder_Graphics &DisableBlending();

    /**
        * @brief Enables alpha blending with expression:
        *      out_color = src_alpha * src_color + (1 - src_alpha) * dst_color
        *      out_alpha = src_alpha
        */
    PipelineBuilder_Graphics &EnableBlendingAlpha();

    /**
        * @brief Enables pre-multiplied alpha blending with expression:
        *      out_color = src_color + (1 - src_alpha) * dst_color
        *      out_alpha = src_alpha
        */
    PipelineBuilder_Graphics &EnableBlendingPremultipliedAlpha();

    /**
        * @brief Enables additive blending with expression:
        *      out_color = src_color + dst_color
        *      out_alpha = src_alpha + dst_alpha
        */
    PipelineBuilder_Graphics &EnableBlendingAdditive();

    PipelineBuilder_Graphics &EnableBlending(VkBlendFactor src_color_blend_factor, VkBlendFactor dst_color_blend_factor, VkBlendOp color_blend_op, VkBlendFactor src_alpha_blend_factor, VkBlendFactor dst_alpha_blend_factor, VkBlendOp alpha_blend_op);    
    
    PipelineBuilder_Graphics &ClearDynamicState();
    PipelineBuilder_Graphics &AddDynamicState(VkDynamicState dynamic_state);

    PipelineBuilder_Graphics &SetLayout(VkPipelineLayout pipeline_layout);

    PipelineBuilder_Graphics &AddColorAttachmentFormat(VkFormat format);
    PipelineBuilder_Graphics &SetDepthStencilAttachmentFormat(VkFormat format);
    PipelineBuilder_Graphics &SetDepthAttachmentFormat(VkFormat format);
    PipelineBuilder_Graphics &SetStencilAttachmentFormat(VkFormat format);
    
    VkPipeline Build();
private:
    VkDevice m_device;
    
    struct ShaderStage {
        VkShaderModule shader_module;
        VkShaderStageFlagBits stage;
    };

    std::vector<ShaderStage> m_stages;

    struct VertexInputState {
        std::vector<VkVertexInputBindingDescription> binding_descriptions;
        std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
    } m_vertex_input_state;

    struct InputAssemblyState {
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkBool32 primitive_restart = false;
    } m_input_assembly_state;

    std::optional<uint32_t> m_patch_control_points = std::nullopt;

    struct RasterizationState {
        VkBool32 discard_all = VK_FALSE;
        VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
        VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
        VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        float line_width = 1.0f;
    } m_rasterization_state;

    struct MultisampleState {
        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        VkBool32 sample_shading_enabled = VK_FALSE;
        float min_sample_shading = 0.0f;
        VkBool32 alpha_to_coverage_enabled = VK_FALSE;
    } m_multisample_state;

    struct DepthState {
        VkBool32 depth_test_enabled = VK_FALSE;
        VkBool32 depth_write_enabled = VK_FALSE;
        VkCompareOp depth_compare = VK_COMPARE_OP_NEVER;
    } m_depth_state;

    struct StencilState {
        VkBool32 stencil_test_enabled = VK_FALSE;
    } m_stencil_state;

    struct ColorBlendState {
        VkBool32 blend_enable = VK_FALSE;
        VkBlendFactor src_color_blend_factor = VK_BLEND_FACTOR_ZERO;
        VkBlendFactor dst_color_blend_factor = VK_BLEND_FACTOR_ZERO;
        VkBlendOp color_blend_op = VK_BLEND_OP_ADD;
        VkBlendFactor src_alpha_blend_factor = VK_BLEND_FACTOR_ZERO;
        VkBlendFactor dst_alpha_blend_factor = VK_BLEND_FACTOR_ZERO;
        VkBlendOp alpha_blend_op = VK_BLEND_OP_ADD;
        VkColorComponentFlags color_write_mask = VK_COLOR_COMPONENT_R_BIT
                                                | VK_COLOR_COMPONENT_G_BIT
                                                | VK_COLOR_COMPONENT_B_BIT
                                                | VK_COLOR_COMPONENT_A_BIT;
    } m_color_blend_state;

    std::vector<VkDynamicState> m_dynamic_state {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    struct AttachmentFormat {
        std::vector<VkFormat> color_formats;
        VkFormat depth_format = VK_FORMAT_UNDEFINED;
        VkFormat stencil_format = VK_FORMAT_UNDEFINED;
    } m_attachment_formats;

    VkPipeline m_base_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
};

class PipelineBuilder_Compute {
public:
    PipelineBuilder_Compute(VkDevice renderer, VkPipelineLayout layout = VK_NULL_HANDLE);

    PipelineBuilder_Compute &SetShaderStage(VkShaderModule shader_module);

    VkPipeline Build();
private:
    struct ShaderStage {
        VkShaderModule shader_module;
    };
private:
    VkDevice m_device;

    ShaderStage m_compute_stage;

    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
};

class PipelineBuilder_RayTracing {
public:
    PipelineBuilder_RayTracing(VkDevice renderer, VkPipelineLayout layout = VK_NULL_HANDLE);

    // Add one of raygen, miss, or callable
    PipelineBuilder_RayTracing &AddGeneralGroup(VkShaderStageFlagBits stage, VkShaderModule shader_module);

    // Closest hit and any hit
    PipelineBuilder_RayTracing &AddTriangleHitGroup(VkShaderModule chit_shader_module, std::optional<VkShaderModule> ahit_shader_module = std::nullopt);

    // Intersect or any hit
    PipelineBuilder_RayTracing &AddProceduralHitGroup(VkShaderModule intersect_shader_module, std::optional<VkShaderModule> ahit_shader_module = std::nullopt);

    PipelineBuilder_RayTracing &SetMaxRayRecursionDepth(uint32_t value = 1);

    PipelineBuilder_RayTracing &ClearDynamicState();
    PipelineBuilder_RayTracing &AddDynamicState(VkDynamicState dynamic_state);

    const std::vector<VkRayTracingShaderGroupCreateInfoKHR> &GetGroups() { return m_groups; }

    VkPipeline Build();
private:
    struct ShaderStage {
        VkShaderModule shader_module;
        VkShaderStageFlagBits stage;
    };
private:
    VkDevice m_device;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

    uint32_t m_max_recursion_depth = 1;

    std::vector<ShaderStage> m_stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_groups;

    std::vector<VkDynamicState> m_dynamic_state {};
};

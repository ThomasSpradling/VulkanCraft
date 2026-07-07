#include "PipelineBuilder.h"
#include "Platform/Graphics/Common.h"
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

/////////////////////////////////////////
// ---- Graphics Pipeline Builder ---- //
/////////////////////////////////////////

PipelineBuilder_Graphics::PipelineBuilder_Graphics(const VulkanDevice &device, VkPipelineLayout layout)
    : m_device(device)
    , m_pipeline_layout(layout)
{}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::FromBase(VkPipeline base_pipeline) {
    m_base_pipeline = base_pipeline;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::AddBinding(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate) {
    m_vertex_input_state.binding_descriptions.push_back({
        .binding = binding,
        .stride = stride,
        .inputRate = input_rate
    });

    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::AddAttribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    m_vertex_input_state.attribute_descriptions.push_back({
        .location = location,
        .binding = binding,
        .format = format,
        .offset = offset,
    });

    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::VertexShader(const CompiledShader &shader, const std::string &entry) {
    return EmplaceShader(ShaderStage::Vertex, m_vertex_shader, shader, entry);
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::FragmentShader(const CompiledShader &shader, const std::string &entry) {
    return EmplaceShader(ShaderStage::Fragment, m_fragment_shader, shader, entry);
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::TessellationControlShader(const CompiledShader &shader, const std::string &entry) {
    return EmplaceShader(ShaderStage::TessellationControl, m_tesselation_control_shader, shader, entry);
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::TessellationEvaluationShader(const CompiledShader &shader, const std::string &entry) {
    return EmplaceShader(ShaderStage::TessellationEvaluation, m_tesselation_eval_shader, shader, entry);
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::GeometryShader(const CompiledShader &shader, const std::string &entry) {
    return EmplaceShader(ShaderStage::Geometry, m_geometry_shader, shader, entry);
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::AddShader(const CompiledShader &shader) {
    if (m_shaders.contains(shader.module_name))
        return *this;

    m_shaders[shader.module_name] = shader;

    const auto fill_shader_entry = [&](std::optional<ShaderEntry> &entry, ShaderStage shader_stage) {
        const auto entries = shader.entry_points.find(shader_stage);
        if (entries == shader.entry_points.end())
            return;

        if (entries->second.empty())
            return;

        entry = ShaderEntry {
            .module_name = shader.module_name,
            .entry_name = entries->second[0],
            .shader_stage = shader_stage,
        };
    };

    fill_shader_entry(m_vertex_shader, ShaderStage::Vertex);
    fill_shader_entry(m_geometry_shader, ShaderStage::Geometry);
    fill_shader_entry(m_tesselation_control_shader, ShaderStage::TessellationControl);
    fill_shader_entry(m_tesselation_eval_shader, ShaderStage::TessellationEvaluation);
    fill_shader_entry(m_fragment_shader, ShaderStage::Fragment);

    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::SetTopology(VkPrimitiveTopology topology) {
    m_input_assembly_state.topology = topology;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::EnablePrimitiveRestart(bool enable_primitive_restart) {
    m_input_assembly_state.primitive_restart = enable_primitive_restart ? VK_TRUE : VK_FALSE;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::SetPatchControlPoints(uint32_t count) {
    m_patch_control_points = count;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::EnableCulling(VkCullModeFlags cull_mode) {
    m_rasterization_state.cull_mode = cull_mode;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::SetFrontFace(VkFrontFace front_face) {
    m_rasterization_state.front_face = front_face;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::DisableCulling() {
    m_rasterization_state.cull_mode = VK_CULL_MODE_NONE;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::SetPolygonMode(VkPolygonMode polygon_mode) {
    m_rasterization_state.polygon_mode = polygon_mode;
    return *this;
}
PipelineBuilder_Graphics &PipelineBuilder_Graphics::SetLineWidth(float width) {
    m_rasterization_state.line_width = width;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::EnableDepthTest(VkCompareOp compare_op) {
    m_depth_state.depth_test_enabled = VK_TRUE;
    m_depth_state.depth_compare = compare_op;
    m_depth_state.depth_write_enabled = VK_TRUE;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::DisableDepthWrite() {
    m_depth_state.depth_write_enabled = VK_FALSE;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::EnableStencilTest() {
    m_stencil_state.stencil_test_enabled = VK_TRUE;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::DisableMSAA() {
    m_multisample_state.sample_count = VK_SAMPLE_COUNT_1_BIT;
    m_multisample_state.sample_shading_enabled = VK_FALSE;
    m_multisample_state.min_sample_shading = 0.0f;
    m_multisample_state.alpha_to_coverage_enabled = VK_FALSE;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::Enable4xMSAA(float min_sample_shading) {
    m_multisample_state.sample_count = VK_SAMPLE_COUNT_4_BIT;
    m_multisample_state.sample_shading_enabled = VK_TRUE;
    m_multisample_state.min_sample_shading = min_sample_shading;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::Enable8xMSAA(float min_sample_shading) {
    m_multisample_state.sample_count = VK_SAMPLE_COUNT_8_BIT;
    m_multisample_state.sample_shading_enabled = VK_TRUE;
    m_multisample_state.min_sample_shading = min_sample_shading;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::EnableAlphaToCoverageMSAA() {
    m_multisample_state.alpha_to_coverage_enabled = VK_TRUE;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::DisableBlending() {
    m_color_blend_state.blend_enable = VK_FALSE;
    m_color_blend_state.src_color_blend_factor = VK_BLEND_FACTOR_ZERO;
    m_color_blend_state.dst_color_blend_factor = VK_BLEND_FACTOR_ZERO;
    m_color_blend_state.src_alpha_blend_factor = VK_BLEND_FACTOR_ZERO;
    m_color_blend_state.dst_alpha_blend_factor = VK_BLEND_FACTOR_ZERO;
    m_color_blend_state.color_blend_op = VK_BLEND_OP_ADD;
    m_color_blend_state.alpha_blend_op = VK_BLEND_OP_ADD;

    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::EnableBlendingAlpha() {
    m_color_blend_state.blend_enable = VK_TRUE;
    m_color_blend_state.src_color_blend_factor = VK_BLEND_FACTOR_SRC_ALPHA;
    m_color_blend_state.dst_color_blend_factor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_color_blend_state.src_alpha_blend_factor = VK_BLEND_FACTOR_ONE;
    m_color_blend_state.dst_alpha_blend_factor = VK_BLEND_FACTOR_ZERO;
    m_color_blend_state.color_blend_op = VK_BLEND_OP_ADD;
    m_color_blend_state.alpha_blend_op = VK_BLEND_OP_ADD;

    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::EnableBlendingPremultipliedAlpha() {
    m_color_blend_state.blend_enable = VK_TRUE;
    m_color_blend_state.src_color_blend_factor = VK_BLEND_FACTOR_ONE;
    m_color_blend_state.dst_color_blend_factor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_color_blend_state.src_alpha_blend_factor = VK_BLEND_FACTOR_ONE;
    m_color_blend_state.dst_alpha_blend_factor = VK_BLEND_FACTOR_ZERO;
    m_color_blend_state.color_blend_op = VK_BLEND_OP_ADD;
    m_color_blend_state.alpha_blend_op = VK_BLEND_OP_ADD;

    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::EnableBlendingAdditive() {
    m_color_blend_state.blend_enable = VK_TRUE;
    m_color_blend_state.src_color_blend_factor = VK_BLEND_FACTOR_ONE;
    m_color_blend_state.dst_color_blend_factor = VK_BLEND_FACTOR_ONE;
    m_color_blend_state.src_alpha_blend_factor = VK_BLEND_FACTOR_ONE;
    m_color_blend_state.dst_alpha_blend_factor = VK_BLEND_FACTOR_ONE;
    m_color_blend_state.color_blend_op = VK_BLEND_OP_ADD;
    m_color_blend_state.alpha_blend_op = VK_BLEND_OP_ADD;

    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::EnableBlending(VkBlendFactor src_color_blend_factor, VkBlendFactor dst_color_blend_factor, VkBlendOp color_blend_op, VkBlendFactor src_alpha_blend_factor, VkBlendFactor dst_alpha_blend_factor, VkBlendOp alpha_blend_op) {
    m_color_blend_state.blend_enable = VK_TRUE;

    m_color_blend_state.src_color_blend_factor = src_color_blend_factor;
    m_color_blend_state.dst_color_blend_factor = dst_color_blend_factor;
    m_color_blend_state.src_alpha_blend_factor = src_alpha_blend_factor;
    m_color_blend_state.dst_alpha_blend_factor = dst_alpha_blend_factor;
    m_color_blend_state.color_blend_op = color_blend_op;
    m_color_blend_state.alpha_blend_op = alpha_blend_op;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::ClearDynamicState() {
    m_dynamic_state.clear();
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::AddDynamicState(VkDynamicState dynamic_state) {
    m_dynamic_state.push_back(dynamic_state);
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::SetLayout(VkPipelineLayout pipeline_layout) {
    m_pipeline_layout = pipeline_layout;
    return *this;
}


PipelineBuilder_Graphics &PipelineBuilder_Graphics::AddColorAttachmentFormat(VkFormat format) {
    m_attachment_formats.color_formats.push_back(format);
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::SetDepthStencilAttachmentFormat(VkFormat format) {
    return SetDepthAttachmentFormat(format)
        .SetStencilAttachmentFormat(format);
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::SetDepthAttachmentFormat(VkFormat format) {
    m_attachment_formats.depth_format = format;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::SetStencilAttachmentFormat(VkFormat format) {
    m_attachment_formats.stencil_format = format;
    return *this;
}

VkPipeline PipelineBuilder_Graphics::Build() {
    VkPipeline pipeline;

    //// Create Shader Stages ////
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages {};
    Assert(m_vertex_shader, "A graphic pipeline must have a vertex shader!");
    Assert(m_fragment_shader, "A graphic pipeline must have a fragment shader!");

    std::unordered_map<std::string, std::unique_ptr<ShaderModule>> shader_modules {};
    for (const auto &[name, shader] : m_shaders) {
        shader_modules[name] = m_device.CreateShaderModule(shader.spirv_code);
    }

    const auto append_shader_stage_info = [&](const std::optional<ShaderEntry> &entry, ShaderStage stage) {
        if (!entry)
            return;

        VkPipelineShaderStageCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = GetVulkanShaderStage(stage),
            .module = shader_modules[entry->module_name]->Handle(),
            .pName = entry->entry_name.c_str(),
            .pSpecializationInfo = nullptr,
        };

        shader_stages.push_back(create_info);
    };
    
    append_shader_stage_info(m_vertex_shader, ShaderStage::Vertex);
    append_shader_stage_info(m_geometry_shader, ShaderStage::Geometry);
    append_shader_stage_info(m_tesselation_control_shader, ShaderStage::TessellationControl);
    append_shader_stage_info(m_tesselation_eval_shader, ShaderStage::TessellationEvaluation);
    append_shader_stage_info(m_fragment_shader, ShaderStage::Fragment);

    assert(shader_stages.size() > 0 && "Cannot create pipeline without any shader stages!");
    assert(m_pipeline_layout != VK_NULL_HANDLE && "Cannot create pipeline without a pipeline layout!");

    //// Get vertex layouts ////
    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertex_input_state.binding_descriptions.size()),
        .pVertexBindingDescriptions = m_vertex_input_state.binding_descriptions.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertex_input_state.attribute_descriptions.size()),
        .pVertexAttributeDescriptions = m_vertex_input_state.attribute_descriptions.data(),
    };

    //// Input assembly ////
    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .topology = m_input_assembly_state.topology,
        .primitiveRestartEnable = m_input_assembly_state.primitive_restart,
    };

    //// Control points ////
    const bool has_tessellation_stage = m_tesselation_control_shader.has_value() || m_tesselation_eval_shader.has_value();
    VkPipelineTessellationStateCreateInfo tessellation_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .patchControlPoints = m_patch_control_points.value_or(3),
    };

    //// Viewport ////
    VkPipelineViewportStateCreateInfo viewport_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    //// Rasterizer ////
    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = m_rasterization_state.discard_all,
        .polygonMode = m_rasterization_state.polygon_mode,
        .cullMode = m_rasterization_state.cull_mode,
        .frontFace = m_rasterization_state.front_face,
        .lineWidth = m_rasterization_state.line_width,
    };

    //// Multisampling ////
    VkPipelineMultisampleStateCreateInfo multisample_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .rasterizationSamples = m_multisample_state.sample_count,
    };

    //// Depth/stencil tests ////
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .depthTestEnable = m_depth_state.depth_test_enabled,
        .depthWriteEnable = m_depth_state.depth_write_enabled,
        .depthCompareOp = m_depth_state.depth_compare,
        .stencilTestEnable = m_stencil_state.stencil_test_enabled,
    };

    //// Color Blending ////
    VkPipelineColorBlendAttachmentState color_blend_attachment {
        .blendEnable = m_color_blend_state.blend_enable,
        .srcColorBlendFactor = m_color_blend_state.src_color_blend_factor,
        .dstColorBlendFactor = m_color_blend_state.dst_color_blend_factor,
        .colorBlendOp = m_color_blend_state.color_blend_op,
        .srcAlphaBlendFactor = m_color_blend_state.src_alpha_blend_factor,
        .dstAlphaBlendFactor = m_color_blend_state.dst_alpha_blend_factor,
        .alphaBlendOp = m_color_blend_state.alpha_blend_op,
        .colorWriteMask = m_color_blend_state.color_write_mask,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };

    //// Dynamic State ////

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .dynamicStateCount = static_cast<uint32_t>(m_dynamic_state.size()),
        .pDynamicStates = m_dynamic_state.data(),
    };

    //// Create Pipeline ////

    VkPipelineRenderingCreateInfoKHR rendering_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<uint32_t>(m_attachment_formats.color_formats.size()),
        .pColorAttachmentFormats = m_attachment_formats.color_formats.data(),
        .depthAttachmentFormat = m_attachment_formats.depth_format,
        .stencilAttachmentFormat = m_attachment_formats.stencil_format,
    };

    VkGraphicsPipelineCreateInfo pipeline_create_info {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_create_info,
        // .flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT,
        .stageCount = static_cast<uint32_t>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_create_info,
        .pTessellationState = has_tessellation_stage ? &tessellation_state_create_info : nullptr,
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState = &multisample_state_create_info,
        .pDepthStencilState = &depth_stencil_state_create_info,
        .pColorBlendState = &color_blend_state_create_info,
        .pDynamicState = &dynamic_state_create_info,
        .layout = m_pipeline_layout,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = m_base_pipeline,
        .basePipelineIndex = 0,
    };

    if (m_base_pipeline != VK_NULL_HANDLE) {
        pipeline_create_info.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        pipeline_create_info.basePipelineIndex = -1;
    }

    VK_CHECK(vkCreateGraphicsPipelines(m_device.Device(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline));

    return pipeline;
}


PipelineBuilder_Graphics &PipelineBuilder_Graphics::EmplaceShader(ShaderStage stage, std::optional<ShaderEntry> &out_entry, const CompiledShader &shader, const std::string &entry) {
    AddShader(shader);

    const auto entries = shader.entry_points.find(stage);
    if (entries == shader.entry_points.end())
        throw std::runtime_error("Shader module " + shader.module_name + " does not contain requested stage!");

    if (std::ranges::find(entries->second, entry) == entries->second.end())
        throw std::runtime_error("There is no entry " + entry + "() in shader module " + shader.module_name);

    out_entry = ShaderEntry {
        .module_name = shader.module_name,
        .entry_name = entry,
        .shader_stage = stage,
    };

    return *this;
}

////////////////////////////////////////
// ---- Compute Pipeline Builder ---- //
////////////////////////////////////////

PipelineBuilder_Compute::PipelineBuilder_Compute(const VulkanDevice &device, VkPipelineLayout layout)
    : m_device(device)
    , m_pipeline_layout(layout)
{}

PipelineBuilder_Compute &PipelineBuilder_Compute::SetShader(const CompiledShader &shader, const std::string &entry_name) {
    const auto entries = shader.entry_points.find(ShaderStage::Compute);
    if (entries == shader.entry_points.end())
        throw std::runtime_error("Shader module " + shader.module_name + " does not contain a compute shader!");

    Assert(entries->second.size() > 0, "Cannot use compute shader without any entries!");

    const std::string entry = entry_name.empty() ? entries->second[0] : entry_name;

    if (m_shader.module_name == shader.module_name && m_compute_shader.entry_name == entry)
        return *this;

    if (std::ranges::find(entries->second, entry) == entries->second.end())
        throw std::runtime_error("Cannot find entry " + entry + "() in module " + shader.module_name);

    m_shader = shader;

    m_compute_shader = ShaderEntry {
        .module_name = shader.module_name,
        .entry_name = entry,
        .shader_stage = ShaderStage::Compute,
    };

    return *this;
}

VkPipeline PipelineBuilder_Compute::Build() {
    std::unique_ptr<ShaderModule> shader_module = m_device.CreateShaderModule(m_shader.spirv_code);

    Assert(m_compute_shader.shader_stage == ShaderStage::Compute, "Expected compute shader to have type of compute!");
    VkPipelineShaderStageCreateInfo shader_stage {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader_module->Handle(),
        .pName = m_compute_shader.entry_name.c_str(),
    };

    VkComputePipelineCreateInfo pipeline_create_info {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .stage = shader_stage,
        .layout = m_pipeline_layout,
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(m_device.Device(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline));
    return pipeline;
}

///////////////////////////////////////////
// ---- RayTracing Pipeline Builder ---- //
///////////////////////////////////////////

PipelineBuilder_RayTracing::PipelineBuilder_RayTracing(const VulkanDevice &device, VkPipelineLayout layout)
    : m_device(device)
    , m_pipeline_layout(layout)
{}

PipelineBuilder_RayTracing &PipelineBuilder_RayTracing::AddShader(const CompiledShader &shader) {
    if (m_shaders.contains(shader.module_name))
        return *this;

    m_shaders[shader.module_name] = shader;
    return *this;
}

PipelineBuilder_RayTracing &PipelineBuilder_RayTracing::AddGeneralGroup(const ModuleEntry &shader) {
    const uint32_t shader_index = ComputeShaderIndex(shader, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR);

    m_groups.push_back({
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .pNext = nullptr,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = shader_index,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    });

    return *this;
}

PipelineBuilder_RayTracing &PipelineBuilder_RayTracing::AddTriangleHitGroup(const ModuleEntry &chit_shader, const ModuleEntry &ahit_shader) {
    const uint32_t chit_index = chit_shader.is_valid ? ComputeShaderIndex(chit_shader, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) : VK_SHADER_UNUSED_KHR;
    const uint32_t ahit_index = ahit_shader.is_valid ? ComputeShaderIndex(ahit_shader, VK_SHADER_STAGE_ANY_HIT_BIT_KHR) : VK_SHADER_UNUSED_KHR;

    if (chit_index == VK_SHADER_UNUSED_KHR && ahit_index == VK_SHADER_UNUSED_KHR)
        throw std::runtime_error("Triangle hit group needs at least a closest-hit or any-hit shader!");

    m_groups.push_back({
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .pNext = nullptr,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = chit_index,
        .anyHitShader = ahit_index,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    });

    return *this;
}

PipelineBuilder_RayTracing &PipelineBuilder_RayTracing::AddProceduralHitGroup(const ModuleEntry &intersection_shader, const ModuleEntry &chit_shader, const ModuleEntry &ahit_shader) {
    const uint32_t intersection_index = ComputeShaderIndex(intersection_shader, VK_SHADER_STAGE_INTERSECTION_BIT_KHR);
    const uint32_t chit_index = chit_shader.is_valid ? ComputeShaderIndex(chit_shader, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) : VK_SHADER_UNUSED_KHR;
    const uint32_t ahit_index = ahit_shader.is_valid ? ComputeShaderIndex(ahit_shader, VK_SHADER_STAGE_ANY_HIT_BIT_KHR) : VK_SHADER_UNUSED_KHR;

    m_groups.push_back({
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .pNext = nullptr,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = chit_index,
        .anyHitShader = ahit_index,
        .intersectionShader = intersection_index,
    });

    return *this;
}

PipelineBuilder_RayTracing &PipelineBuilder_RayTracing::SetLayout(VkPipelineLayout pipeline_layout) {
    m_pipeline_layout = pipeline_layout;
    return *this;
}

PipelineBuilder_RayTracing &PipelineBuilder_RayTracing::SetMaxRayRecursionDepth(uint32_t value) {
    m_max_recursion_depth = value;
    return *this;
}

PipelineBuilder_RayTracing &PipelineBuilder_RayTracing::ClearDynamicState() {
    m_dynamic_state.clear();
    return *this;
}

PipelineBuilder_RayTracing &PipelineBuilder_RayTracing::AddDynamicState(VkDynamicState dynamic_state) {
    m_dynamic_state.push_back(dynamic_state);
    return *this;
}

VkPipeline PipelineBuilder_RayTracing::Build() {
    Assert(!m_shader_entries.empty(), "Cannot create raytracing pipeline without shader stages!");
    Assert(!m_groups.empty(), "Cannot create raytracing pipeline without shader groups!");
    Assert(m_pipeline_layout != VK_NULL_HANDLE, "Cannot create raytracing pipeline without a pipeline layout!");

    //// Create Shader Modules ////

    std::unordered_map<std::string, std::unique_ptr<ShaderModule>> shader_modules {};
    for (const auto &[name, shader] : m_shaders) {
        shader_modules[name] = m_device.CreateShaderModule(shader.spirv_code);
    }

    //// Create Shader Stages ////

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages {};
    shader_stages.reserve(m_shader_entries.size());

    for (const ShaderEntry &entry : m_shader_entries) {
        shader_stages.push_back({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .stage = GetVulkanShaderStage(entry.shader_stage),
            .module = shader_modules[entry.module_name]->Handle(),
            .pName = entry.entry_name.c_str(),
            .pSpecializationInfo = nullptr,
        });
    }

    //// Dynamic State ////

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .dynamicStateCount = static_cast<uint32_t>(m_dynamic_state.size()),
        .pDynamicStates = m_dynamic_state.data(),
    };

    //// Create Pipeline ////

    VkRayTracingPipelineCreateInfoKHR pipeline_create_info {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .stageCount = static_cast<uint32_t>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .groupCount = static_cast<uint32_t>(m_groups.size()),
        .pGroups = m_groups.data(),
        .maxPipelineRayRecursionDepth = m_max_recursion_depth,
        .pDynamicState = &dynamic_state_create_info,
        .layout = m_pipeline_layout,
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRayTracingPipelinesKHR(m_device.Device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline));
    return pipeline;
}

uint32_t PipelineBuilder_RayTracing::ComputeShaderIndex(const ModuleEntry &entry, VkShaderStageFlags allowed_stages) {
    if (!entry.is_valid)
        throw std::runtime_error("Invalid raytracing shader entry!");

    for (uint32_t i = 0; i < m_shader_entries.size(); i++) {
        const ShaderEntry &shader_entry = m_shader_entries[i];

        if (shader_entry.module_name != entry.module_name)
            continue;

        if (shader_entry.entry_name != entry.entry_point)
            continue;

        const VkShaderStageFlags stage = GetVulkanShaderStage(shader_entry.shader_stage);
        if ((stage & allowed_stages) != 0)
            return i;
    }

    const auto shader = m_shaders.find(entry.module_name);
    if (shader == m_shaders.end())
        throw std::runtime_error("Shader module '" + entry.module_name + "' was not added to the raytracing pipeline!");

    for (const auto &[shader_stage, entries] : shader->second.entry_points) {
        const VkShaderStageFlags vulkan_stage = GetVulkanShaderStage(shader_stage);
        if ((vulkan_stage & allowed_stages) == 0)
            continue;

        for (const std::string &entry_name : entries) {
            if (entry_name != entry.entry_point)
                continue;

            auto index = static_cast<uint32_t>(m_shader_entries.size());

            m_shader_entries.push_back({
                .module_name = entry.module_name,
                .entry_name = entry.entry_point,
                .shader_stage = shader_stage,
            });

            return index;
        }
    }

    throw std::runtime_error("Could not find raytracing entry " + entry.entry_point + "() in shader module '" + entry.module_name + "'!");
}

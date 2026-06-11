#include "PipelineBuilder.h"
#include <cassert>

/////////////////////////////////////////
// ---- Graphics Pipeline Builder ---- //
/////////////////////////////////////////

PipelineBuilder_Graphics::PipelineBuilder_Graphics(VkDevice device, VkPipelineLayout layout)
    : m_device(device)
    , m_pipeline_layout(layout)
{}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::FromBase(VkPipeline base_pipeline) {
    m_base_pipeline = base_pipeline;
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::AddBinding(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate) {
    m_vertex_input_state.binding_descriptions.push_back({
        binding,
        stride,
        input_rate
    });

    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::AddAttribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    m_vertex_input_state.attribute_descriptions.push_back({
        location,
        binding,
        format,
        offset,
    });

    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::AddShaderStage(VkShaderStageFlagBits shader_stage, VkShaderModule shader_module) {
    m_stages.push_back({ shader_module, shader_stage });
    return *this;
}

PipelineBuilder_Graphics &PipelineBuilder_Graphics::AddTesselationStage(VkShaderModule shader_control_module, VkShaderModule shader_eval_module, uint32_t patch_control_points) {
    SetTopology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
    
    AddShaderStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, shader_control_module);
    AddShaderStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, shader_eval_module);
    m_patch_control_points = patch_control_points;

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
    m_stencil_state.stencil_test_enabled = VK_FALSE;
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

    // (1) Create shader stages

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    shader_stages.reserve(m_stages.size());
    for (const auto &stage : m_stages) {
        VkPipelineShaderStageCreateInfo stage_create_info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage.stage,
            .module = stage.shader_module,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        };
        shader_stages.push_back(stage_create_info);
    }

    assert(shader_stages.size() > 0 && "Cannot create pipeline without any shader stages!");
    assert(m_pipeline_layout != VK_NULL_HANDLE && "Cannot create pipeline without a pipeline layout!");

    // (2) Get vertex layouts

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertex_input_state.binding_descriptions.size()),
        .pVertexBindingDescriptions = m_vertex_input_state.binding_descriptions.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertex_input_state.attribute_descriptions.size()),
        .pVertexAttributeDescriptions = m_vertex_input_state.attribute_descriptions.data(),
    };

    // (3) Input assembly

    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .topology = m_input_assembly_state.topology,
        .primitiveRestartEnable = m_input_assembly_state.primitive_restart,
    };

    // (4) Control points
    const bool has_tessellation_stage = m_patch_control_points.has_value();

    VkPipelineTessellationStateCreateInfo tessellation_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .patchControlPoints = has_tessellation_stage ? *m_patch_control_points : 1,
    };

    // (5) Viewport
    VkPipelineViewportStateCreateInfo viewport_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    // (6) Rasterizer
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

    // (7) Multisampling
    VkPipelineMultisampleStateCreateInfo multisample_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .rasterizationSamples = m_multisample_state.sample_count,
    };

    // (8) Depth/stencil tests
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .depthTestEnable = m_depth_state.depth_test_enabled,
        .depthWriteEnable = m_depth_state.depth_write_enabled,
        .depthCompareOp = m_depth_state.depth_compare,
        .stencilTestEnable = m_stencil_state.stencil_test_enabled,
    };

    // (9) Color blending
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

    // (10) Dynamic state

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .dynamicStateCount = static_cast<uint32_t>(m_dynamic_state.size()),
        .pDynamicStates = m_dynamic_state.data(),
    };

    // Finally: Putting it all together

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

    vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline);

    return pipeline;
}

////////////////////////////////////////
// ---- Compute Pipeline Builder ---- //
////////////////////////////////////////

PipelineBuilder_Compute::PipelineBuilder_Compute(VkDevice device, VkPipelineLayout layout)
    : m_device(device)
    , m_pipeline_layout(layout)
{}

PipelineBuilder_Compute &PipelineBuilder_Compute::SetShaderStage(VkShaderModule shader_module) {
    m_compute_stage.shader_module = shader_module;
    return *this;
}

VkPipeline PipelineBuilder_Compute::Build() {
    VkPipelineShaderStageCreateInfo stage_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = m_compute_stage.shader_module,
        .pName = "main",
        .pSpecializationInfo = nullptr,
    };

    VkComputePipelineCreateInfo pipeline_create_info {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .stage = stage_create_info,
        .layout = m_pipeline_layout,
    };

    VkPipeline pipeline;
    vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline);
    return pipeline;
}

///////////////////////////////////////////
// ---- RayTracing Pipeline Builder ---- //
///////////////////////////////////////////

PipelineBuilder_RayTracing::PipelineBuilder_RayTracing(VkDevice device, VkPipelineLayout layout)
    : m_device(device)
    , m_pipeline_layout(layout)
{}

PipelineBuilder_RayTracing &PipelineBuilder_RayTracing::AddGeneralGroup(VkShaderStageFlagBits stage, VkShaderModule shader_module) {
    m_stages.push_back({
        .shader_module = shader_module,
        .stage = stage,
    });

    VkRayTracingShaderGroupCreateInfoKHR group_create_info {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = uint32_t(m_stages.size() - 1),
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    };

    m_groups.push_back(group_create_info);

    return *this;
}

PipelineBuilder_RayTracing &PipelineBuilder_RayTracing::AddTriangleHitGroup(VkShaderModule chit_shader_module, std::optional<VkShaderModule> ahit_shader_module) {
    m_stages.push_back({
        .shader_module = chit_shader_module,
        .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
    });
    uint32_t chit_index = uint32_t(m_stages.size() - 1);

    uint32_t ahit_index = VK_SHADER_UNUSED_KHR;
    if (ahit_shader_module.has_value()) {
        m_stages.push_back({
            .shader_module = *ahit_shader_module,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        });
        ahit_index = uint32_t(m_stages.size() - 1);
    }

    VkRayTracingShaderGroupCreateInfoKHR group_create_info {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = chit_index,
        .anyHitShader = ahit_index,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    };

    m_groups.push_back(group_create_info);
    return *this;
}

PipelineBuilder_RayTracing &PipelineBuilder_RayTracing::AddProceduralHitGroup(VkShaderModule intersect_shader_module, std::optional<VkShaderModule> ahit_shader_module) {
    uint32_t intersect_index = uint32_t(m_stages.size() - 1);
    m_stages.push_back({
        .shader_module = intersect_shader_module,
        .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
    });

    uint32_t ahit_index = VK_SHADER_UNUSED_KHR;
    if (ahit_shader_module.has_value()) {
        m_stages.push_back({
            .shader_module = *ahit_shader_module,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        });
        ahit_index = uint32_t(m_stages.size() - 1);
    }

    VkRayTracingShaderGroupCreateInfoKHR group_create_info {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = ahit_index,
        .intersectionShader = intersect_index,
    };

    m_groups.push_back(group_create_info);
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

    // (1) Create shader stages

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    shader_stages.reserve(m_stages.size());
    for (const auto &stage : m_stages) {
        VkPipelineShaderStageCreateInfo stage_create_info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage.stage,
            .module = stage.shader_module,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        };
        shader_stages.push_back(stage_create_info);
    }

    // (2) Dynamic state

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .dynamicStateCount = static_cast<uint32_t>(m_dynamic_state.size()),
        .pDynamicStates = m_dynamic_state.data(),
    };

    VkRayTracingPipelineCreateInfoKHR raytracing_pipeline_create_info {
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

    VkPipeline pipeline;
    vkCreateRayTracingPipelinesKHR(m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracing_pipeline_create_info, nullptr, &pipeline);
    return pipeline;
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <volk.h>
#include <vector>

#include "ShaderCompiler.h"
#include "VulkanDevice.h"
#include "VulkanObjects.h"

struct ShaderEntry {
    std::string module_name;
    std::string entry_name;
    ShaderStage shader_stage;
};

enum class BlendMode : uint8_t {
    Disabled,
    Alpha,
    Premultiplied,
    Additive,
};

struct ColorAttachment {
    VkFormat format = VK_FORMAT_UNDEFINED;
    BlendMode blending_mode = BlendMode::Disabled;
};

class VulkanPipeline;
class PipelineBuilder_Graphics {
public:
    PipelineBuilder_Graphics(const VulkanDevice &device);
    
    PipelineBuilder_Graphics &AddBinding(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX);
    PipelineBuilder_Graphics &AddAttribute(uint32_t location, uint32_t binding, DataFormat format, uint32_t offset = 0);

    PipelineBuilder_Graphics &FromBase(VkPipeline base_pipeline);

    PipelineBuilder_Graphics &AddShader(const CompiledShader &shader);

    PipelineBuilder_Graphics &VertexShader(const CompiledShader &shader, const std::string &entry);
    PipelineBuilder_Graphics &GeometryShader(const CompiledShader &shader, const std::string &entry);
    PipelineBuilder_Graphics &TessellationControlShader(const CompiledShader &shader, const std::string &entry);
    PipelineBuilder_Graphics &TessellationEvaluationShader(const CompiledShader &shader, const std::string &entry);
    PipelineBuilder_Graphics &TaskShader(const CompiledShader &shader, const std::string &entry);
    PipelineBuilder_Graphics &MeshShader(const CompiledShader &shader, const std::string &entry);
    PipelineBuilder_Graphics &FragmentShader(const CompiledShader &shader, const std::string &entry);
    
    PipelineBuilder_Graphics &SetTopology(VkPrimitiveTopology topology);
    PipelineBuilder_Graphics &EnablePrimitiveRestart(bool enable_primitive_restart);

    PipelineBuilder_Graphics &SetPatchControlPoints(uint32_t count);

    PipelineBuilder_Graphics &EnableCulling(VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT);
    PipelineBuilder_Graphics &SetFrontFace(VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE);
    PipelineBuilder_Graphics &DisableCulling();

    PipelineBuilder_Graphics &SetPolygonMode(VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL);
    PipelineBuilder_Graphics &SetLineWidth(float width);
    PipelineBuilder_Graphics &SetDepthBias(float constant, float clamp, float slope);
    PipelineBuilder_Graphics &DisableDepthBias();

    PipelineBuilder_Graphics &EnableDepthTest(VkCompareOp compare_op = VK_COMPARE_OP_LESS);
    PipelineBuilder_Graphics &DisableDepthTest();
    PipelineBuilder_Graphics &DisableDepthWrite();
    PipelineBuilder_Graphics &EnableStencilTest();

    PipelineBuilder_Graphics &DisableMSAA();
    PipelineBuilder_Graphics &Enable4xMSAA(float min_sample_shading = 0.2f);
    PipelineBuilder_Graphics &Enable8xMSAA(float min_sample_shading = 1.0f);
    PipelineBuilder_Graphics &EnableAlphaToCoverageMSAA();

    PipelineBuilder_Graphics &AddColorAttachment(ColorAttachment attachment);
    PipelineBuilder_Graphics &SetDepthStencilAttachmentFormat(VkFormat format);
    PipelineBuilder_Graphics &SetDepthAttachmentFormat(VkFormat format);
    PipelineBuilder_Graphics &SetStencilAttachmentFormat(VkFormat format);

    PipelineBuilder_Graphics &AddDynamicState(VkDynamicState dynamic_state);
    
    PipelineBuilder_Graphics &AddPushConstant(const VkPushConstantRange &range);
    PipelineBuilder_Graphics &AddDescriptorSetLayout(const VkDescriptorSetLayout &layout);
    PipelineBuilder_Graphics &SetSpecializationConstants(VkSpecializationInfo &info);

    std::unique_ptr<VulkanPipeline> Build();
private:
    const VulkanDevice &m_device;

    std::unordered_map<std::string, CompiledShader> m_shaders {};

    std::optional<ShaderEntry> m_vertex_shader;
    std::optional<ShaderEntry> m_geometry_shader;
    std::optional<ShaderEntry> m_tesselation_control_shader;
    std::optional<ShaderEntry> m_tesselation_eval_shader;
    std::optional<ShaderEntry> m_task_shader;
    std::optional<ShaderEntry> m_mesh_shader;
    std::optional<ShaderEntry> m_fragment_shader;

    struct VertexInputState {
        std::vector<VkVertexInputBindingDescription> binding_descriptions;
        std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
    } m_vertex_input_state;

    struct InputAssemblyState {
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkBool32 primitive_restart = false;
    } m_input_assembly_state;

    std::optional<uint32_t> m_patch_control_points = std::nullopt;

    VkPipelineRasterizationStateCreateInfo m_rasterization_state_create_info = VkPipelineRasterizationStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f,
    };

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
        VkFormat depth_format = VK_FORMAT_UNDEFINED;
    } m_depth_state;

    struct StencilState {
        VkBool32 stencil_test_enabled = VK_FALSE;
        VkFormat stencil_format = VK_FORMAT_UNDEFINED;
    } m_stencil_state;

    std::vector<VkFormat> m_attachment_color_formats;
    std::vector<VkPipelineColorBlendAttachmentState> m_attachment_blend_state;

    std::vector<VkDynamicState> m_dynamic_state {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    struct AttachmentFormat {
        std::vector<VkFormat> color_formats;
        VkFormat depth_format = VK_FORMAT_UNDEFINED;
        VkFormat stencil_format = VK_FORMAT_UNDEFINED;
    } m_attachment_formats;

    std::vector<VkPushConstantRange> m_push_constants;
    std::vector<VkDescriptorSetLayout> m_descriptor_layouts;
    VkSpecializationInfo *m_specialization_constants = nullptr;

    VkPipeline m_base_pipeline = VK_NULL_HANDLE;
private:
    PipelineBuilder_Graphics &EmplaceShader(ShaderStage stage, std::optional<ShaderEntry> &out_entry, const CompiledShader &shader, const std::string &entry);
};

class PipelineBuilder_Compute {
public:
    PipelineBuilder_Compute(const VulkanDevice &device);

    PipelineBuilder_Compute &SetShader(const CompiledShader &shader, const std::string &entry_name = "");

    PipelineBuilder_Compute &AddPushConstant(const VkPushConstantRange &range);
    PipelineBuilder_Compute &AddDescriptorSetLayout(const VkDescriptorSetLayout &layout);
    PipelineBuilder_Compute &SetSpecializationConstants(VkSpecializationInfo &info);

    std::unique_ptr<VulkanPipeline> Build();
private:
    const VulkanDevice &m_device;

    CompiledShader m_shader;
    ShaderEntry m_compute_shader;

    std::vector<VkPushConstantRange> m_push_constants;
    std::vector<VkDescriptorSetLayout> m_descriptor_layouts;
    VkSpecializationInfo *m_specialization_constants = nullptr;
};

struct ModuleEntry {
    bool is_valid = false;

    std::string module_name;
    std::string entry_point;

    ModuleEntry() = default;
    ModuleEntry(std::nullptr_t) {}

    ModuleEntry(const CompiledShader &shader, std::string entry)
        : is_valid(true)
        , module_name(shader.module_name)
        , entry_point(std::move(entry)) {}
};

class PipelineBuilder_RayTracing {
public:
    PipelineBuilder_RayTracing(const VulkanDevice &device);

    PipelineBuilder_RayTracing &AddShader(const CompiledShader &shader);

    PipelineBuilder_RayTracing &AddGeneralGroup(const ModuleEntry &shader);
    PipelineBuilder_RayTracing &AddTriangleHitGroup(const ModuleEntry &chit_shader = nullptr, const ModuleEntry &ahit_shader = nullptr);
    PipelineBuilder_RayTracing &AddProceduralHitGroup(const ModuleEntry &intersection_shader, const ModuleEntry &chit_shader = nullptr, const ModuleEntry &ahit_shader = nullptr);

    PipelineBuilder_RayTracing &SetMaxRayRecursionDepth(uint32_t value = 1);

    PipelineBuilder_RayTracing &ClearDynamicState();
    PipelineBuilder_RayTracing &AddDynamicState(VkDynamicState dynamic_state);

    const std::vector<VkRayTracingShaderGroupCreateInfoKHR> &GetGroups() const { return m_groups; }
    
    PipelineBuilder_RayTracing &AddPushConstant(const VkPushConstantRange &range);
    PipelineBuilder_RayTracing &AddDescriptorSetLayout(const VkDescriptorSetLayout &layout);
    PipelineBuilder_RayTracing &SetSpecializationConstants(VkSpecializationInfo &info);

    std::unique_ptr<VulkanPipeline> Build();
private:
    const VulkanDevice &m_device;

    std::unordered_map<std::string, CompiledShader> m_shaders {};
    std::vector<ShaderEntry> m_shader_entries {};
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_groups {};
    std::vector<VkDynamicState> m_dynamic_state {};

    uint32_t m_max_recursion_depth = 1;

    std::vector<VkPushConstantRange> m_push_constants;
    std::vector<VkDescriptorSetLayout> m_descriptor_layouts;
    VkSpecializationInfo *m_specialization_constants = nullptr;
private:
    uint32_t ComputeShaderIndex(const ModuleEntry &entry, VkShaderStageFlags allowed_stages);
};

class VulkanPipeline {
    friend PipelineBuilder_Graphics;
    friend PipelineBuilder_Compute;
    friend PipelineBuilder_RayTracing;
public:
    VulkanPipeline(const VulkanDevice &device);
    ~VulkanPipeline();

    static inline PipelineBuilder_Graphics GraphicsBuilder(const VulkanDevice &device) { return PipelineBuilder_Graphics(device); }
    static inline PipelineBuilder_Compute ComputeBuilder(const VulkanDevice &device)  { return PipelineBuilder_Compute(device); }
    static inline PipelineBuilder_RayTracing RayTracingBuilder(const VulkanDevice &device)  { return PipelineBuilder_RayTracing(device); }

    VkPipeline Pipeline() const { return m_pipeline; };
    VkPipelineBindPoint BindPoint() const { return m_pipeline_type; };
    VkPipelineLayout Layout() const { return m_pipeline_layout; };

    void SetDebugName(std::string_view name);
private:
    const VulkanDevice &m_device;

    VkPipelineBindPoint m_pipeline_type;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
};

#pragma once

#include <glm/glm.hpp>
#include <variant>
#include "Core/Handle.h"
#include "Platform/Graphics/VulkanBuffer.h"

enum class AlphaMode : uint8_t {
    Opaque = 0,
    Mask,
    Blend,
};

struct MetallicRoughnessMaterial {
    struct TextureInfo {
        TextureHandle texture = TextureHandle::Invalid();
        SamplerHandle sampler = SamplerHandle::Invalid();
        uint32_t texcoord = 0;
    };

    glm::vec4 base_color_factor = glm::vec4(1.0f);
    TextureInfo base_color_texture {};

    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    
    TextureInfo metallic_roughness_texture {};

    TextureInfo normal_texture {}; // tangent space
    float normal_texture_scale = 1.0f;

    TextureInfo occlusion_texture {};
    float occlusion_texture_strength = 1.0f;

    TextureInfo emissive_texture {};
    glm::vec3 emissive_factor = glm::vec3(0.0f);

    AlphaMode alpha_mode;
    float alpha_cutoff = 0.5f;
    bool double_sided = false;
};

struct BasicMaterial {
    glm::vec4 color = glm::vec4(1.0f);

    TextureHandle color_texture = TextureHandle::Invalid();
    SamplerHandle color_sampler = SamplerHandle::Invalid();
};

using Material = std::variant<MetallicRoughnessMaterial, BasicMaterial>;

enum class MaterialType : uint8_t {
    MetallicRoughness,
    Basic,
};

struct MaterialGpuBinding {
    const VulkanBuffer *buffer = nullptr;
    uint32_t material_index = 0;
    MaterialType type = MaterialType::Basic;
};

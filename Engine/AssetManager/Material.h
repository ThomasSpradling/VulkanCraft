#pragma once

#include <glm/glm.hpp>
#include <variant>
#include "Core/Handle.h"
#include "Platform/Graphics/VulkanBuffer.h"

struct MetallicRoughnessMaterial {
    glm::vec4 albedo = glm::vec4(1.0f);

    TextureHandle albedo_texture = TextureHandle::Invalid();
    SamplerHandle albedo_sampler = SamplerHandle::Invalid();
    uint32_t albedo_texcoord = 0;

    TextureHandle normal_texture = TextureHandle::Invalid();
    SamplerHandle normal_sampler = SamplerHandle::Invalid();
    uint32_t normal_texcoord = 0;
    float normal_scale = 1.0f;

    float metallic = 0.0f;
    float roughness = 0.0f;
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

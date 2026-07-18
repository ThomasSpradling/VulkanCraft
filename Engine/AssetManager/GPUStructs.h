#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <volk.h>

using TextureId = uint32_t;
using SamplerId = uint32_t;
using StorageImageId = uint32_t;
using AccelerationStructureId = uint32_t;

//// Materials ////

struct alignas(16) GPUMetallicRoughnessMaterial {
    glm::vec4 base_color_factor = glm::vec4(1.0f);

    TextureId base_color_texture = 0;
    SamplerId base_color_sampler = 0;
    uint32_t base_color_texcoord = 0;
    float metallic_factor = 1.0f;

    float roughness_factor = 1.0f;
    TextureId metallic_roughness_texture = 0;
    SamplerId metallic_roughness_sampler = 0;
    uint32_t metallic_roughness_texcoord = 0;

    TextureId normal_texture = 0;
    SamplerId normal_sampler = 0;
    uint32_t normal_texcoord = 0;
    float normal_texture_scale = 1.0f;

    TextureId occlusion_texture = 0;
    SamplerId occlusion_sampler = 0;
    uint32_t occlusion_texcoord = 0;
    float occlusion_texture_strength = 1.0f;

    TextureId emissive_texture = 0;
    SamplerId emissive_sampler = 0;
    uint32_t emissive_texcoord = 0;
    float emissive_factor_x = 0.0f;

    float emissive_factor_y = 0.0f;
    float emissive_factor_z = 0.0f;
    float alpha_cutoff = 0.5f;
    uint32_t double_sided = 0;
};

struct GPUBasicMaterial {
    glm::vec4 color = glm::vec4(1.0f);

    TextureId color_texture = 0;
    SamplerId color_sampler = 0;
};

//// Lights ////
struct alignas(16) GPUDirectionalLight {
    glm::vec4 color; // w = intensity
    glm::vec3 direction;
    float pad_;
};

struct alignas(16) GPUPointLight {
    glm::vec4 color; // w = intensity
    glm::vec3 position;
    float range;
};

struct GPULightData {
    GPUDirectionalLight directional_light;

    uint32_t point_light_count;
    float pad_;
    VkDeviceAddress point_lights;
};

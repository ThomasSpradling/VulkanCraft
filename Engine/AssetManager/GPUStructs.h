#pragma once

#include <cstdint>
#include <glm/glm.hpp>

using TextureId = uint32_t;
using SamplerId = uint32_t;
using StorageImageId = uint32_t;
using AccelerationStructureId = uint32_t;

struct alignas(16) GPUMetallicRoughnessMaterial {
    glm::vec4 base_color = glm::vec4(1.0f);

    float metallic = 0.0f;
    float roughness = 1.0f;

    TextureId albedo_texture = 0;
    SamplerId albedo_sampler = 0;
    uint32_t albedo_texcoord = 0;
    
    TextureId normal_texture = 0;
    SamplerId normal_sampler = 0;
    uint32_t normal_texcoord = 0;
    float normal_texture_scale = 0;

    uint32_t padding[3] {};
};

struct GPUBasicMaterial {
    glm::vec4 color = glm::vec4(1.0f);

    TextureId color_texture = 0;
    SamplerId color_sampler = 0;
};

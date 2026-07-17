#pragma once

#include <cstddef>
#include <glm/glm.hpp>
#include <vector>

enum class TextureFormat : uint8_t {
    R8,
    RG8,
    RGBA8,
    RGBA8Srgb,
    RGBA16Float,
    RGBA32Float,
};

struct TextureData {
    glm::uvec2 extent {};
    TextureFormat format = TextureFormat::RGBA8;
    std::vector<std::byte> pixels;
    bool generate_mipmaps = false;
};

enum class SamplerFilter : uint8_t {
    Nearest,
    Linear,
};

enum class TextureWrapMode : uint8_t {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
};

struct TextureSamplerData {
    SamplerFilter min_filter;
    SamplerFilter mag_filter;
    SamplerFilter mipmap_filter;

    TextureWrapMode wrap_u;
    TextureWrapMode wrap_v;

    bool use_mipmaps = false;
};

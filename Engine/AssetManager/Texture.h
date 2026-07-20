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

struct Texture {
    glm::uvec2 extent {};
    TextureFormat format = TextureFormat::RGBA8;
    std::vector<std::byte> pixels;
    bool generate_mipmaps = false;

    static Texture Checkerboard(
        glm::uvec2 extent = glm::uvec2(256),
        uint32_t square_size = 32,
        glm::u8vec4 color0 = glm::u8vec4(255),
        glm::u8vec4 color1 = glm::u8vec4(0, 0, 0, 255),
        TextureFormat format = TextureFormat::RGBA8Srgb
    );
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

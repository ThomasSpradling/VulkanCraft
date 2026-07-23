#pragma once

#include <cstddef>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include <volk.h>

enum class TextureFormat : uint8_t {
    Invalid = 0,

    R8_Unorm,
    R16_Unsigned_Int,
    R32_Unsigned_Int,
    R16_Unorm,
    R16_Float,
    R32_Float,
    
    RG8_Unorm,
    RG16_Unsigned_Int,
    RG32_Unsigned_Int,
    RG16_Unorm,
    RG16_Float,
    RG32_Float,

    RGBA8_Unorm,
    RGBA32_Unsigned_Int,
    RGBA16_Float,
    RGBA32_Float,
    RGBA8_sRGB,

    BGRA8_Unorm,
    BGRA8_sRGB,

    R8 = R8_Unorm,
    RGBA8 = RGBA8_Unorm,
};

enum class Encoding : uint8_t {
    Invalid = 0,

    Unorm,
    Unsigned_Int,
    Float,
    sRGB,
};

uint32_t ChannelCount(TextureFormat format);
size_t ChannelSize(TextureFormat format);
Encoding FormatEncoding(TextureFormat format);

TextureFormat InterpretSRGB(TextureFormat format);
TextureFormat InterpretUNorm(TextureFormat format);
TextureFormat InterpretUnsignedInteger(TextureFormat format);

VkFormat GetVulkanFormat(TextureFormat format);

enum class ImageType : uint8_t {
    Image2D,
    Image3D,
    ImageCube,
};

struct Texture {
    ImageType type = ImageType::Image2D;
    glm::uvec3 extent {};
    uint32_t layers = 1;
    TextureFormat format = TextureFormat::RGBA8;
    std::vector<std::byte> pixels;
    bool generate_mipmaps = false;

    static Texture Checkerboard(
        glm::uvec2 extent = glm::uvec2(256),
        uint32_t square_size = 32,
        glm::u8vec4 color0 = glm::u8vec4(255),
        glm::u8vec4 color1 = glm::u8vec4(0, 0, 0, 255),
        TextureFormat format = TextureFormat::RGBA8_sRGB
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

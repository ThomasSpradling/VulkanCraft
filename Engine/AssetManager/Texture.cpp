#include "Texture.h"
#include <iostream>
#include <stdexcept>

uint32_t ChannelCount(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_Unorm:
        case TextureFormat::R16_Unsigned_Int:
        case TextureFormat::R32_Unsigned_Int:
        case TextureFormat::R16_Unorm:
        case TextureFormat::R16_Float:
        case TextureFormat::R32_Float:
            return 1;
        case TextureFormat::RG8_Unorm:
        case TextureFormat::RG16_Unsigned_Int:
        case TextureFormat::RG32_Unsigned_Int:
        case TextureFormat::RG16_Unorm:
        case TextureFormat::RG16_Float:
        case TextureFormat::RG32_Float:
            return 2;
        case TextureFormat::RGBA8_Unorm:
        case TextureFormat::RGBA32_Unsigned_Int:
        case TextureFormat::RGBA16_Float:
        case TextureFormat::RGBA32_Float:
        case TextureFormat::RGBA8_sRGB:
        case TextureFormat::BGRA8_Unorm:
        case TextureFormat::BGRA8_sRGB:
            return 4;
        default:
    };

    throw std::runtime_error("Invalid texture format!");
}

size_t ChannelSize(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_Unorm:
        case TextureFormat::RG8_Unorm:
        case TextureFormat::RGBA8_Unorm:
        case TextureFormat::BGRA8_Unorm:
        case TextureFormat::RGBA8_sRGB:
        case TextureFormat::BGRA8_sRGB:
            return 1;
        case TextureFormat::R16_Unsigned_Int:
        case TextureFormat::R16_Unorm:
        case TextureFormat::R16_Float:
        case TextureFormat::RG16_Unsigned_Int:
        case TextureFormat::RG16_Unorm:
        case TextureFormat::RG16_Float:
        case TextureFormat::RGBA16_Float:
            return 2;
        case TextureFormat::R32_Unsigned_Int:
        case TextureFormat::R32_Float:
        case TextureFormat::RG32_Unsigned_Int:
        case TextureFormat::RG32_Float:
        case TextureFormat::RGBA32_Unsigned_Int:
        case TextureFormat::RGBA32_Float:
            return 4;
        default:
    };

    throw std::runtime_error("Invalid texture format!");
}

Encoding FormatEncoding(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_Unorm:
        case TextureFormat::RG8_Unorm:
        case TextureFormat::R16_Unorm:
        case TextureFormat::RG16_Unorm:
        case TextureFormat::RGBA8_Unorm:
        case TextureFormat::BGRA8_Unorm:
            return Encoding::Unorm;
        case TextureFormat::R16_Float:
        case TextureFormat::R32_Float:
        case TextureFormat::RG16_Float:
        case TextureFormat::RG32_Float:
        case TextureFormat::RGBA16_Float:
        case TextureFormat::RGBA32_Float:
            return Encoding::Float;
        case TextureFormat::R16_Unsigned_Int:
        case TextureFormat::R32_Unsigned_Int:
        case TextureFormat::RG16_Unsigned_Int:
        case TextureFormat::RG32_Unsigned_Int:
        case TextureFormat::RGBA32_Unsigned_Int:
            return Encoding::Unsigned_Int;
        case TextureFormat::RGBA8_sRGB:
        case TextureFormat::BGRA8_sRGB:
            return Encoding::sRGB;
        default:
            return Encoding::Invalid;
    };
}

TextureFormat InterpretSRGB(TextureFormat format) {
    if (FormatEncoding(format) == Encoding::sRGB)
        return format;

    switch (format) {
        case TextureFormat::RGBA8_Unorm:
            return TextureFormat::RGBA8_sRGB;
        case TextureFormat::BGRA8_Unorm:
            return TextureFormat::BGRA8_sRGB;
        default:
            throw std::runtime_error("Has no supported sRGB equivalent!");
    };
}

TextureFormat InterpretUNorm(TextureFormat format) {
    if (FormatEncoding(format) == Encoding::Unorm)
        return format;

    switch (format) {
        case TextureFormat::RGBA8_sRGB:
        case TextureFormat::BGRA8_sRGB:
            return TextureFormat::RGBA8_Unorm;

        case TextureFormat::R16_Unsigned_Int:
            return TextureFormat::R16_Unorm;
            
        case TextureFormat::RG16_Unsigned_Int:
            return TextureFormat::RG16_Unorm;
            
        default:
            throw std::runtime_error("Has no supported unsigned normalized integer equivalent!");
    };
}

TextureFormat InterpretUnsignedInteger(TextureFormat format) {
    if (FormatEncoding(format) == Encoding::Unsigned_Int)
        return format;

    switch (format) {
        case TextureFormat::R16_Unorm:
            return TextureFormat::R16_Unorm;
            
        case TextureFormat::RG16_Unorm:
            return TextureFormat::RG16_Unsigned_Int;
            
        default:
            throw std::runtime_error("Has no supported unsigned integer equivalent!");
    };
}

VkFormat GetVulkanFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_Unorm:           return VK_FORMAT_R8_UNORM;
        case TextureFormat::R16_Unorm:          return VK_FORMAT_R16_UNORM;
        case TextureFormat::R16_Unsigned_Int:   return VK_FORMAT_R16_UINT;
        case TextureFormat::R32_Unsigned_Int:   return VK_FORMAT_R32_UINT;
        case TextureFormat::R16_Float:          return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::R32_Float:          return VK_FORMAT_R32_SFLOAT;

        case TextureFormat::RG8_Unorm:          return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::RG16_Unorm:         return VK_FORMAT_R16G16_UNORM;
        case TextureFormat::RG16_Unsigned_Int:  return VK_FORMAT_R16G16_UINT;
        case TextureFormat::RG32_Unsigned_Int:  return VK_FORMAT_R32G32_UINT;
        case TextureFormat::RG16_Float:         return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RG32_Float:         return VK_FORMAT_R32G32_SFLOAT;

        case TextureFormat::RGBA8_Unorm:        return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::BGRA8_Unorm:        return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::RGBA32_Unsigned_Int: return VK_FORMAT_R32G32B32A32_UINT;
        case TextureFormat::RGBA16_Float:       return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::RGBA32_Float:       return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::RGBA8_sRGB:         return VK_FORMAT_R8G8B8A8_SRGB;
        case TextureFormat::BGRA8_sRGB:         return VK_FORMAT_B8G8R8A8_SRGB;
            
        case TextureFormat::Invalid:
        default:
            std::cerr << "Tried converting format to VK_FORMAT_UNDEFINED\n";
            return VK_FORMAT_UNDEFINED;
    };
}

Texture Texture::Checkerboard(glm::uvec2 extent, uint32_t square_size, glm::u8vec4 color0, glm::u8vec4 color1, TextureFormat format) {
    Texture texture {
        .extent = glm::uvec3(extent, 1.0f),
        .format = format,
        .generate_mipmaps = true,
    };

    square_size = std::max(square_size, 1u);
    texture.pixels.resize(static_cast<size_t>(extent.x) * static_cast<size_t>(extent.y) * 4);

    for (uint32_t y = 0; y < extent.y; ++y) {
        for (uint32_t x = 0; x < extent.x; ++x) {
            bool alternate = ((x / square_size) + (y / square_size)) % 2 != 0;

            glm::u8vec4 color = alternate ? color1 : color0;
            size_t index = (static_cast<size_t>(y) * extent.x + x) * 4;

            texture.pixels[index + 0] = static_cast<std::byte>(color.r);
            texture.pixels[index + 1] = static_cast<std::byte>(color.g);
            texture.pixels[index + 2] = static_cast<std::byte>(color.b);
            texture.pixels[index + 3] = static_cast<std::byte>(color.a);
        }
    }

    return texture;
}

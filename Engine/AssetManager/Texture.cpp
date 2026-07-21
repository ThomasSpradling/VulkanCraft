#include "Texture.h"

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

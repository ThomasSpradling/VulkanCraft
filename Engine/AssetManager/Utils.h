#include "Bitmap.h"
#include <chrono>
#include <glm/common.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>

template<typename Format, size_t ChannelCount>
std::vector<Bitmap<Format, ChannelCount>> ConvertEquirectangularToCubeMap(const Bitmap<Format, ChannelCount> &bitmap) {
    using Pixel = typename Bitmap<Format, ChannelCount>::Pixel;

    if (bitmap.Width() == 0 || bitmap.Height() == 0 || bitmap.Width() % 4 != 0 || bitmap.Width() / 2 != bitmap.Height()) {
        throw std::runtime_error("Expected a 2:1 equirectangular bitmap with a width divisible by four");
    }

    uint32_t face_size = bitmap.Width() / 4;
    std::vector<Bitmap<Format, ChannelCount>> result {};
    result.reserve(6);

    // Face IDs are in order +X, -X, +Y, -Y, +Z, -Z
    const auto face_coords_to_position = [](glm::vec2 uv, uint32_t face_id) {
        // Convention as usual is -Z being forward
        float u = 2.0f * uv.x - 1.0f;
        float v = 1.0f - 2.0f * uv.y;

        switch (face_id) {
            case 0: return glm::vec3( 1.0f,     v,    -u); // +X
            case 1: return glm::vec3(-1.0f,     v,     u); // -X
            case 2: return glm::vec3(    u,  1.0f,    -v); // +Y
            case 3: return glm::vec3(    u, -1.0f,     v); // -Y
            case 4: return glm::vec3(    u,     v,  1.0f); // +Z
            case 5: return glm::vec3(   -u,     v, -1.0f); // -Z
            default: return glm::vec3(0.0f);
        }
    };

    const auto sample_pixel = [&](float x, float y) -> Pixel {
        float width = static_cast<float>(bitmap.Width());
        x -= std::floor(x / width) * width;
        y = glm::clamp(y, 0.0f, static_cast<float>(bitmap.Height() - 1));

        uint32_t x1 = static_cast<uint32_t>(std::floor(x));
        uint32_t y1 = static_cast<uint32_t>(std::floor(y));
        uint32_t x2 = (x1 + 1) % bitmap.Width();
        uint32_t y2 = std::min(y1 + 1, bitmap.Height() - 1);

        float s = x - static_cast<float>(x1);
        float t = y - static_cast<float>(y1);

        Pixel bottom_left = bitmap.GetPixel(x1, y1);
        Pixel bottom_right = bitmap.GetPixel(x2, y1);
        Pixel top_left = bitmap.GetPixel(x1, y2);
        Pixel top_right = bitmap.GetPixel(x2, y2);
        Pixel result {};

        for (uint32_t channel = 0; channel < ChannelCount; ++channel) {
            float value =
                static_cast<float>(bottom_left[channel]) * (1.0f - s) * (1.0f - t) +
                static_cast<float>(bottom_right[channel]) * s * (1.0f - t) +
                static_cast<float>(top_left[channel]) * (1.0f - s) * t +
                static_cast<float>(top_right[channel]) * s * t;

            if constexpr (std::is_integral_v<Format>) {
                value = glm::clamp(std::round(value), static_cast<float>(std::numeric_limits<Format>::lowest()), static_cast<float>(std::numeric_limits<Format>::max()));
            }

            result[channel] = static_cast<Format>(value);
        }

        return result;
    };

    for (uint32_t face = 0; face < 6; ++face) {
        result.emplace_back(face_size, face_size);
    }

    float inverse_face_size = 1.0f / static_cast<float>(face_size);

    for (uint32_t face = 0; face < 6; ++face) {
        for (uint32_t y = 0; y < face_size; ++y) {
            for (uint32_t x = 0; x < face_size; ++x) {
                float s = (static_cast<float>(x) + 0.5f) * inverse_face_size;
                float t = (static_cast<float>(y) + 0.5f) * inverse_face_size;

                glm::vec3 direction = glm::normalize(face_coords_to_position(glm::vec2(s, t), face));
                float longitude = std::atan2(direction.x, -direction.z);
                float latitude = std::asin(glm::clamp(direction.y, -1.0f, 1.0f));

                float u = longitude / (2.0f * glm::pi<float>()) + 0.5f;
                float v = 0.5f - latitude / glm::pi<float>();
                float image_x = u * static_cast<float>(bitmap.Width()) - 0.5f;
                float image_y = v * static_cast<float>(bitmap.Height()) - 0.5f;

                result[face].SetPixel(x, y, sample_pixel(image_x, image_y));
            }
        }
    }

    return result;
}

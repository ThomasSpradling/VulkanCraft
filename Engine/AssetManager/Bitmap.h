#pragma once

#include "AssetManager/Texture.h"
#include <bit>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

#include <stb_image.h>

template<typename Format, size_t ChannelCount = 4>
class Bitmap {
public:
    using Pixel = glm::vec<ChannelCount, Format>;
    static constexpr std::size_t PixelBytes = sizeof(Format) * ChannelCount;
public:
    Bitmap(uint32_t width, uint32_t height)
        : m_width(width)
        , m_height(height)
        , m_data(static_cast<size_t>(width) * static_cast<size_t>(height) * PixelBytes)
    {};

    Bitmap(uint32_t width, uint32_t height, const void *ptr)
        : m_width(width)
        , m_height(height)
    {
        size_t byte_size = static_cast<size_t>(width) * static_cast<size_t>(height) * PixelBytes;
        m_data.resize(byte_size);
        std::memcpy(m_data.data(), ptr, byte_size);
    }

    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }

    Pixel GetPixel(uint32_t x, uint32_t y) const {
        size_t index = (x + m_width * y) * sizeof(Format) * ChannelCount;
        Pixel pixel {};
        std::memcpy(glm::value_ptr(pixel), m_data.data() + index, PixelBytes);
        return pixel;
    }

    void SetPixel(uint32_t x, uint32_t y, const Pixel &pixel) {
        size_t index = (x + m_width * y) * sizeof(Format) * ChannelCount;
        std::memcpy(m_data.data() + index, glm::value_ptr(pixel), PixelBytes);
    }

    static Bitmap<Format, ChannelCount> Load(const std::filesystem::path &path) {
        if constexpr (std::is_same_v<float, Format>) {
            int width = 0;
            int height = 0;
            int num_channels = 0;
    
            float *data = stbi_loadf(path.string().c_str(), &width, &height, &num_channels, ChannelCount);
            if (data == nullptr || width == 0 || height == 0 || num_channels == 0)
                throw std::runtime_error(std::format("Failed to load texture '{}': {}", path.string(), stbi_failure_reason()));
    
            Assert(ChannelCount != num_channels, std::format("Loaded texture '{}' has {} channels, but you requested {} channels!", path.string(), num_channels, ChannelCount));
        
            return Bitmap<Format, ChannelCount>(static_cast<uint32_t>(width), static_cast<uint32_t>(height), data);
        } else {
            int width = 0;
            int height = 0;
            int num_channels = 0;
    
            unsigned char *data = stbi_load(path.string().c_str(), &width, &height, &num_channels, ChannelCount);
            if (data == nullptr || width == 0 || height == 0 || num_channels == 0)
                throw std::runtime_error(std::format("Failed to load texture '{}': {}", path.string(), stbi_failure_reason()));

            Assert(ChannelCount == num_channels, std::format("Loaded texture '{}' has {} channels, but you requested {} channels!", path.string(), num_channels, ChannelCount));
            return Bitmap<Format, ChannelCount>(static_cast<uint32_t>(width), static_cast<uint32_t>(height), data);
        }
    }

    TextureFormat GetFormat() {
        static_assert(ChannelCount == 1 || ChannelCount == 2 || ChannelCount == 4, "Invalid channel count!");

        if constexpr (ChannelCount == 1) {
            if constexpr (std::is_same_v<Format, uint8_t>)
                return TextureFormat::R8_Unorm;

            if constexpr (std::is_same_v<Format, uint16_t>)
                return TextureFormat::R16_Unsigned_Int;

            if constexpr (std::is_same_v<Format, uint32_t>)
                return TextureFormat::R32_Unsigned_Int;

            if constexpr (std::is_same_v<Format, float>)
                return TextureFormat::R32_Float;
        }

        if constexpr (ChannelCount == 2) {
            if constexpr (std::is_same_v<Format, uint8_t>)
                return TextureFormat::RG8_Unorm;

            if constexpr (std::is_same_v<Format, uint16_t>)
                return TextureFormat::RG16_Unsigned_Int;

            if constexpr (std::is_same_v<Format, uint32_t>)
                return TextureFormat::RG32_Unsigned_Int;

            if constexpr (std::is_same_v<Format, float>)
                return TextureFormat::RG32_Float;
        }

        if constexpr (ChannelCount == 4) {
            if constexpr (std::is_same_v<Format, uint8_t>)
                return TextureFormat::RGBA8_Unorm;

            if constexpr (std::is_same_v<Format, uint32_t>)
                return TextureFormat::RGBA32_Unsigned_Int;

            if constexpr (std::is_same_v<Format, float>)
                return TextureFormat::RGBA32_Float;
        }
    }

    std::vector<std::byte> &Data() { return m_data; }
    const std::vector<std::byte> &Data() const { return m_data; }
private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<std::byte> m_data;
};

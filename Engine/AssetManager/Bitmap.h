#pragma once

#include <bit>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

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

    Bitmap(std::vector<std::byte> data, uint32_t width, uint32_t height)
        : m_width(width)
        , m_height(height)
        , m_data(std::move(data))
    {
        size_t expected_size = static_cast<size_t>(width) * static_cast<size_t>(height) * PixelBytes;
        Assert(m_data.size() == expected_size, "Invalid size!");
    }

    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }

    Pixel GetPixel(uint32_t x, uint32_t y) const {
        // Assert(x < m_width && y < m_height, "Out of range!");

        size_t index = (x + m_width * y) * sizeof(Format) * ChannelCount;
        Pixel pixel {};
        std::memcpy(glm::value_ptr(pixel), m_data.data() + index, PixelBytes);
        return pixel;
    }

    void SetPixel(uint32_t x, uint32_t y, const Pixel &pixel) {
        // Assert(x < m_width && y < m_height, "Out of range!");

        size_t index = (x + m_width * y) * sizeof(Format) * ChannelCount;
        std::memcpy(m_data.data() + index, glm::value_ptr(pixel), PixelBytes);
    }

    std::vector<std::byte> &Data() { return m_data; }
    const std::vector<std::byte> &Data() const { return m_data; }
private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<std::byte> m_data;
};

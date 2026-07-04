#pragma once

#include <cstddef>
#include <filesystem>
#include <glm/glm.hpp>
#include <vector>

enum class ImageEncoding {
    Unknown,
    sRGB,
    Linear,
    LinearFloat,
};

enum class ImagePixelFormat {
    Unknown,
    R8,         // 8-bit uint, single channel
    RGBA8,      // 8-bit uint, four channels
    RGBA32f,    // 32-bit float, four channels
};

class Image {
public:
    ImageEncoding Encoding() const { return m_image_encoding; }
    ImagePixelFormat Format() const { return m_image_pixel_format; }
    const std::vector<std::byte> &Data() const { return m_data; }
protected:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    ImageEncoding m_image_encoding = ImageEncoding::Unknown;
    ImagePixelFormat m_image_pixel_format = ImagePixelFormat::Unknown;

    std::vector<std::byte> m_data {};
};

class Image2D : public Image {
public:
    void Load(const std::filesystem::path &path, ImageEncoding encoding = ImageEncoding::sRGB);
private:

};

class Image3D : public Image {
public:
private:
    uint32_t m_depth = 0;
};

class Image2DArray : public Image {
private:
    uint32_t m_layers = 0;
};

class Image2DCube : public Image {

};

class Image2DCubeArray : public Image {
private:
    uint32_t m_layers = 0;
};

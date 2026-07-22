#include "AssetManager.h"
#include "AssetManager/Texture.h"
#include "Bitmap.h"
#include "Buffer.h"
#include "Core/Handle.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Platform/Graphics/Common.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanImage.h"
#include "GLTFModel.h"
#include "Texture.h"

#include "Utils.h"
#include <functional>
#include <future>
#include <iostream>
#include <sstream>
#include <stb_image.h>
#include <stdexcept>

AssetManager::AssetManager(VulkanDevice &device, BindlessDescriptorTable &descriptor_table)
    : m_device(device)
    , m_bindless_table(descriptor_table)
{
    m_pbr_material_buffer = CreateBuffer(GPUBufferData {
        .usage = BufferUsageBits::Storage,
        .storage_type = StorageType::Device,
        .size = sizeof(GPUMetallicRoughnessMaterial) * MaxPbrMaterials,
    });

    m_basic_material_buffer = CreateBuffer(GPUBufferData {
        .usage = BufferUsageBits::Storage,
        .storage_type = StorageType::Device,
        .size = sizeof(GPUBasicMaterial) * MaxBasicMaterials,
    });

    GPUMetallicRoughnessMaterial default_pbr {};
    GPUBasicMaterial default_basic {};

    GetBuffer(m_pbr_material_buffer).UploadAt(&default_pbr, 0);
    GetBuffer(m_basic_material_buffer).UploadAt(&default_basic, 0);
}

AssetManager::~AssetManager() {
    m_gltf_models.Clear();
    m_meshes.Clear();
    m_buffers.Clear();
}

GLTFHandle AssetManager::LoadGLTF(const std::filesystem::path &path) {
    auto current_model = std::make_unique<GLTFModel>(*this, path);
    return m_gltf_models.Add(current_model);
}

MeshHandle AssetManager::CreateMesh(const std::vector<MeshVertex> &vertices, const std::vector<uint32_t> &indices) {
    auto current_mesh = std::make_unique<Mesh>(*this, vertices, indices);
    return m_meshes.Add(current_mesh);
}

MeshHandle AssetManager::CreateMesh(const Shape &shape) {
    std::vector<MeshVertex> vertices(shape.VertexCount());
    std::ranges::transform(shape.Vertices(), vertices.begin(), [](const ShapeVertex &vert) {
        return MeshVertex {
            .position = vert.position,
            .normal = vert.normal,
            .tangent = vert.tangent,
            .uv0 = vert.uv,
            .uv1 = vert.uv,
        };
    });

    return CreateMesh(vertices, shape.Indices());
}

const GLTFModel &AssetManager::GetGLTF(GLTFHandle gltf) {
    return *m_gltf_models.Get(gltf);
}

const Mesh &AssetManager::GetMesh(MeshHandle mesh) {
    return *m_meshes.Get(mesh);
}

void AssetManager::DestroyMesh(MeshHandle handle) {
    std::unique_ptr<Mesh> mesh = m_meshes.TakeOwnership(handle);
    mesh.reset();
}

void AssetManager::DestroyGLTF(GLTFHandle handle) {
    std::unique_ptr<GLTFModel> mesh = m_gltf_models.TakeOwnership(handle);
    mesh.reset();
}

MaterialHandle AssetManager::CreateMaterial(const MetallicRoughnessMaterial &material) {
    GPUMetallicRoughnessMaterial gpu_material {
        .base_color_factor = material.base_color_factor,

        .base_color_texture = GetTextureId(material.base_color_texture.texture),
        .base_color_sampler = GetSamplerId(material.base_color_texture.sampler),
        .base_color_texcoord = material.base_color_texture.texcoord,

        .metallic_factor = material.metallic_factor,
        .roughness_factor = material.roughness_factor,

        .metallic_roughness_texture = GetTextureId(material.metallic_roughness_texture.texture),
        .metallic_roughness_sampler = GetSamplerId(material.metallic_roughness_texture.sampler),
        .metallic_roughness_texcoord = material.metallic_roughness_texture.texcoord,

        .normal_texture = GetTextureId(material.normal_texture.texture),
        .normal_sampler = GetSamplerId(material.normal_texture.sampler),
        .normal_texcoord = material.normal_texture.texcoord,
        .normal_texture_scale = material.normal_texture_scale,

        .occlusion_texture = GetTextureId(material.occlusion_texture.texture),
        .occlusion_sampler = GetSamplerId(material.occlusion_texture.sampler),
        .occlusion_texcoord = material.occlusion_texture.texcoord,
        .occlusion_texture_strength = material.occlusion_texture_strength,

        .emissive_texture = GetTextureId(material.emissive_texture.texture),
        .emissive_sampler = GetSamplerId(material.emissive_texture.sampler),
        .emissive_texcoord = material.emissive_texture.texcoord,
        .emissive_factor_x = material.emissive_factor.x,
        .emissive_factor_y = material.emissive_factor.y,
        .emissive_factor_z = material.emissive_factor.z,

        .alpha_cutoff = material.alpha_cutoff,
        .double_sided = material.double_sided ? 1u : 0u,
    };

    if (m_pbr_material_buffer_index + 1 < MaxPbrMaterials) {
        m_pbr_material_buffer_index++;
        GetBuffer(m_pbr_material_buffer).UploadAt(&gpu_material, m_pbr_material_buffer_index);
    }

    MaterialRecord record {
        .material_index = m_pbr_material_buffer_index,
        .type = MaterialType::MetallicRoughness,
    };
    return m_materials.Add(record);
}

MaterialHandle AssetManager::CreateMaterial(const BasicMaterial &material) {
    GPUBasicMaterial gpu_material {
        .color = material.color,
        .color_texture = GetTextureId(material.color_texture),
        .color_sampler = GetSamplerId(material.color_sampler),
    };

    if (m_basic_material_buffer_index + 1 < MaxBasicMaterials) {
        m_basic_material_buffer_index++;
        GetBuffer(m_basic_material_buffer).UploadAt(&gpu_material, m_basic_material_buffer_index);
    }

    MaterialRecord record {
        .material_index = m_basic_material_buffer_index,
        .type = MaterialType::Basic,
    };
    return m_materials.Add(record);
}

const AssetManager::MaterialRecord &AssetManager::GetMaterial(MaterialHandle material) {
    return m_materials.Get(material);
}

void AssetManager::DestroyMaterial(MaterialHandle handle) {
    // no op
}

SamplerHandle AssetManager::CreateSampler(const TextureSamplerData &sampler) {
    const auto get_filter = [](SamplerFilter filter) -> VkFilter {
        switch (filter) {
            case SamplerFilter::Nearest:
                return VK_FILTER_NEAREST;
            default:
            case SamplerFilter::Linear:
                return VK_FILTER_LINEAR;
        }
    };

    const auto get_mipmap_filter = [](SamplerFilter filter) -> VkSamplerMipmapMode {
        switch (filter) {
            case SamplerFilter::Nearest:
                return VK_SAMPLER_MIPMAP_MODE_NEAREST;
            case SamplerFilter::Linear:
            default:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
    };

    const auto get_address_mode = [](TextureWrapMode mode) -> VkSamplerAddressMode {
        switch (mode) {
            case TextureWrapMode::ClampToEdge:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case TextureWrapMode::MirroredRepeat:
                return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case TextureWrapMode::Repeat:
            default:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    };

    VkSamplerCreateInfo sampler_create_info {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = get_filter(sampler.mag_filter),
        .minFilter = get_filter(sampler.min_filter),
        .mipmapMode = get_mipmap_filter(sampler.mipmap_filter),
        .addressModeU = get_address_mode(sampler.wrap_u),
        .addressModeV = get_address_mode(sampler.wrap_v),
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .minLod = 0.0f,
        .maxLod = sampler.use_mipmaps ? VK_LOD_CLAMP_NONE : 0.0f,
    };

    VkSampler gpu_sampler;
    VK_CHECK(vkCreateSampler(m_device.Device(), &sampler_create_info, nullptr, &gpu_sampler));

    SamplerId sampler_id = m_bindless_table.AddSampler(gpu_sampler);

    SamplerRecord record {
        .sampler_id = sampler_id,
    };
    return m_samplers.Add(record);
}

std::pair<SamplerId, VkSampler> AssetManager::GetSampler(SamplerHandle sampler) {
    SamplerId sampler_id = m_samplers.Get(sampler).sampler_id;
    VkSampler vulkan_sampler = m_bindless_table.GetSampler(sampler_id);
    return std::make_pair(sampler_id, vulkan_sampler);
}

TextureHandle AssetManager::LoadTexture2D(const std::filesystem::path &path, TextureFormat format, bool generate_mipmaps) {
    auto [extent, data] = LoadImageData(path, format);

    Texture texture {
        .type = ImageType::Image2D,
        .extent = glm::uvec3(extent, 1),
        .layers = 1,
        .format = format,
        .pixels = std::move(data),
        .generate_mipmaps = generate_mipmaps,
    };

    return CreateTexture(texture);
}

TextureHandle AssetManager::LoadTextureCubeFromEquirectangular(const std::filesystem::path &path, TextureFormat format) {
    auto [extent, data] = LoadImageData(path, format);
    
    Assert((extent.x % 4) == 0 || extent.x / 2 == extent.y, "Invalid cube map!");
    
    const auto load_cube = [&]<typename Format, size_t ChannelCount>() -> TextureHandle {
        using BitmapType = Bitmap<Format, ChannelCount>;

        const auto expected_data_size = static_cast<size_t>(extent.x) * static_cast<size_t>(extent.y) * BitmapType::PixelBytes;
        Assert(data.size() == expected_data_size, "Non-matching formats!");

        BitmapType bitmap (std::move(data), extent.x, extent.y);
        auto faces = ConvertEquirectangularToCubeMap(bitmap);

        const uint32_t face_size = faces.front().Width();
        size_t face_byte_size = static_cast<size_t>(face_size) * static_cast<size_t>(face_size) * BitmapType::PixelBytes;
        
        std::vector<std::byte> layered_data;
        layered_data.reserve(face_byte_size * 6);

        for (const BitmapType &face : faces) {
            const std::vector<std::byte> &bytes = face.Data();
            layered_data.insert(layered_data.end(), bytes.begin(), bytes.end());
        }

        Texture texture {
            .type = ImageType::ImageCube,
            .extent = glm::uvec3(face_size, face_size, 1),
            .layers = 6,
            .format = format,
            .pixels = std::move(layered_data),
            .generate_mipmaps = false,
        };

        return CreateTexture(texture);
    };

    switch (format) {
        case TextureFormat::R8:
            return load_cube.operator()<uint8_t, 1>();

        case TextureFormat::RG8:
            return load_cube.operator()<uint8_t, 2>();

        case TextureFormat::RGBA8:
        case TextureFormat::RGBA8Srgb:
            return load_cube.operator()<uint8_t, 4>();

        case TextureFormat::RGB32Float:
            return load_cube.operator()<float, 3>();
        case TextureFormat::RGBA32Float:
            return load_cube.operator()<float, 4>();
    }

    return TextureHandle::Invalid();
}

TextureHandle AssetManager::CreateTexture(const Texture &texture) {
    bool is_cube_map = false;

    const auto get_texture_format = [](TextureFormat format) -> VkFormat {
        switch (format) {
            case TextureFormat::R8:
                return VK_FORMAT_R8_UNORM;
            case TextureFormat::RG8:
                return VK_FORMAT_R8G8_UNORM;
            case TextureFormat::RGBA8Srgb:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case TextureFormat::RGB32Float:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case TextureFormat::RGBA32Float:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case TextureFormat::RGBA8:
            default:
                return VK_FORMAT_R8G8B8A8_UNORM;
        }
    };

    Assert(texture.layers > 0, "A texture must have at least one layer!");

    auto gpu_texture_builder = VulkanImage::ImageBuilder(m_device)
        .Format(get_texture_format(texture.format))
        .AddUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    switch (texture.type) {
        case ImageType::Image2D:
            if (texture.layers == 1) {
                gpu_texture_builder.Image2D(texture.extent.x, texture.extent.y);
            } else {
                gpu_texture_builder.Image2DArray(texture.extent.x, texture.extent.y, texture.layers);
            }
            break;
        case ImageType::Image3D:
            Assert(texture.layers == 1, "Currently we only support 3D textures with one layer.");
            gpu_texture_builder.Image3D(texture.extent.x, texture.extent.y, texture.extent.z);
            break;
        case ImageType::ImageCube:
            Assert(texture.layers % 6 == 0, "A cube map must have a multiple of six layers!");
            Assert(texture.extent.x == texture.extent.y, "A cube map must have equal side lengths!");
            is_cube_map = true;
            if (texture.layers == 6) {
                gpu_texture_builder.CubeMap(texture.extent.x);
            } else {
                gpu_texture_builder.CubeMapArray(texture.extent.x, texture.layers / 6);
            }
            break;
    }

    if (texture.generate_mipmaps)
        gpu_texture_builder.MipMaps();
    
    auto gpu_texture = gpu_texture_builder.Build();
    gpu_texture->TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    gpu_texture->Upload(TextureRange {
        .dimensions = texture.extent,
        .layer = 0,
        .num_layers = texture.layers,
        .mip_levels = 0,
        .num_mip_levels = 1,
    }, texture.pixels.data(), sizeof(std::byte) * texture.extent.x);

    m_device.ImmediateSubmit(QueueType::Graphics, [&](const CommandBuffer &cmd) {
        if (texture.generate_mipmaps) {
            cmd.GenerateMipMaps(*gpu_texture, VK_FILTER_LINEAR);
        }
        cmd.ImageMemoryBarrier(*gpu_texture)
            .DestAccess(VK_ACCESS_2_SHADER_SAMPLED_READ_BIT)
            .DestStage(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR)
            .TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .Execute();
    });

    TextureRecord record {};
    if (is_cube_map) {
        record.cube_texture_id = m_bindless_table.AddTextureCube(gpu_texture);
    } else {
        record.texture_id = m_bindless_table.AddTexture(gpu_texture);
    }
    return m_textures.Add(record);
}

BufferHandle AssetManager::CreateBuffer(const GPUBufferData &buffer) {
    auto builder = VulkanBuffer::BufferBuilder(m_device);
    
    std::stringstream debug_name;

    switch (buffer.storage_type) {
        case StorageType::Device: {
            builder.AddUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            builder.AddUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT);

            debug_name << "Device Local";
            break;
        }
        case StorageType::HostVisible: {
            builder.AddMemoryFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT);
            builder.AddMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

            debug_name << "Host Visible";
            break;
        }
        case StorageType::MemoryLess: {
            debug_name << "Memoryless";
            break;
        }
    }

    if (static_cast<uint8_t>(buffer.usage & BufferUsageBits::Vertex))
        builder.AddUsage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    if (static_cast<uint8_t>(buffer.usage & BufferUsageBits::Index))
        builder.AddUsage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    if (static_cast<uint8_t>(buffer.usage & BufferUsageBits::Uniform)) {
        builder.AddUsage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        builder.AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    }

    if (static_cast<uint8_t>(buffer.usage & BufferUsageBits::Storage)) {
        builder.AddUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        builder.AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        builder.AddUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }

    if (static_cast<uint8_t>(buffer.usage & BufferUsageBits::Indirect)) {
        builder.AddUsage(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        builder.AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    }

    debug_name << " Buffer";

    if (!buffer.debug_name.empty()) {
        debug_name << ": " << buffer.debug_name;
    }

    builder.Size(buffer.size);

    std::unique_ptr<VulkanBuffer> vk_buffer = builder.Build();
    if (buffer.data != nullptr)
        vk_buffer->Upload(buffer.data, buffer.size);

    vk_buffer->SetDebugName(debug_name.str());
    return m_buffers.Add(vk_buffer);
}

VulkanBuffer &AssetManager::GetBuffer(BufferHandle buffer) {
    return *m_buffers.Get(buffer);
}

const VulkanBuffer &AssetManager::GetBuffer(BufferHandle buffer) const {
    return *m_buffers.Get(buffer);
}

void AssetManager::DestroyBuffer(BufferHandle handle) {
    std::unique_ptr<VulkanBuffer> buffer = m_buffers.TakeOwnership(handle);

    m_device.GetGarbageCollector().Enqueue(std::packaged_task<void()>([buffer = std::move(buffer)]() mutable {
        buffer.reset();
    }));
}

std::pair<TextureId, std::reference_wrapper<VulkanImage>> AssetManager::GetTexture(TextureHandle texture) {
    Assert(texture != TextureHandle::Invalid(), "Must have valid texture handle!");

    TextureRecord record = m_textures.Get(texture);
    if (record.cube_texture_id != 0) {
        VulkanImage &image = m_bindless_table.GetTextureCube(record.cube_texture_id);
        return std::make_pair(record.cube_texture_id, std::reference_wrapper(image));
    } else if (record.texture_id != 0) {
        VulkanImage &image = m_bindless_table.GetTexture(record.texture_id);
        return std::make_pair(record.texture_id, std::reference_wrapper(image));
    }

    throw std::runtime_error("Invalid texture!");
}

TextureId AssetManager::GetTextureId(TextureHandle texture) {
    if (!texture)
        return DefaultTextureId;

    return m_textures.Get(texture).texture_id;
}

void AssetManager::DestroyTexture(TextureHandle handle) {
    TextureRecord image = m_textures.TakeOwnership(handle);

    m_device.GetGarbageCollector().Enqueue(std::packaged_task<void()>([this, image]() {
        m_bindless_table.RemoveTexture(image.texture_id);
    }));
}

SamplerId AssetManager::GetSamplerId(SamplerHandle sampler) {
    if (!sampler)
        return DefaultSamplerId;

    return m_samplers.Get(sampler).sampler_id;
}

void AssetManager::DestroySampler(SamplerHandle handle) {
    SamplerRecord sampler = m_samplers.TakeOwnership(handle);

    m_device.GetGarbageCollector().Enqueue(std::packaged_task<void()>([this, sampler]() {
        m_bindless_table.RemoveSampler(sampler.sampler_id);
    }));
}

const VulkanBuffer &AssetManager::MaterialBuffer(MaterialType type) const {
    switch (type) {
        case MaterialType::MetallicRoughness:
            return GetBuffer(m_pbr_material_buffer);

        case MaterialType::Basic:
            return GetBuffer(m_basic_material_buffer);
    }

    std::unreachable();
}

std::pair<glm::uvec2, std::vector<std::byte>> AssetManager::LoadImageData(const std::filesystem::path &path, TextureFormat &format) {
    bool is_floating_point = false;
    int desired_channels = 0;
    uint32_t channel_size = 0;
    switch (format) {
        case TextureFormat::R8: {
            desired_channels = 1;
            channel_size = 1;
            break;
        }
        case TextureFormat::RG8: {
            desired_channels = 2;
            channel_size = 1;
            break;
        }
        case TextureFormat::RGBA8: {
            desired_channels = 4;
            channel_size = 1;
            break;
        }
        case TextureFormat::RGBA8Srgb: {
            desired_channels = 4;
            channel_size = 1;
            break;
        }
        case TextureFormat::RGB32Float: {
            is_floating_point = true;
            desired_channels = 3;
            channel_size = 4;
            break;
        }
        case TextureFormat::RGBA32Float: {
            is_floating_point = true;
            desired_channels = 4;
            channel_size = 4;
            break;
        }
    }

    int width = 0;
    int height = 0;
    int channel_count = 0;

    std::vector<std::byte> result {};
    
    if (is_floating_point) {
        float *data = stbi_loadf(path.string().c_str(), &width, &height, &channel_count, desired_channels);
        if (data == nullptr || width == 0 || height == 0 || channel_count == 0)
            throw std::runtime_error(std::format("Failed to load texture '{}': {}", path.string(), stbi_failure_reason()));

        Assert(channel_count == desired_channels, "We currently only support floating point images with 4 channels");

        size_t byte_count = static_cast<size_t>(width) * static_cast<size_t>(height) * channel_count * channel_size;
        result.resize(byte_count);
        std::memcpy(result.data(), data, byte_count);

        stbi_image_free(data);
    } else {
        unsigned char *data = stbi_load(path.string().c_str(), &width, &height, &channel_count, STBI_rgb_alpha);
        if (data == nullptr || width == 0 || height == 0 || channel_count == 0)
            throw std::runtime_error(std::format("Failed to load texture '{}': {}", path.string(), stbi_failure_reason()));

        if (channel_count != desired_channels) {
            std::cerr << "Warning: Cannot get desired channel count for texture " << path.string() << ". Falling back.\n";
            if (channel_count == 1)
                format = TextureFormat::R8;
            if (channel_count == 2)
                format = TextureFormat::RG8;
            if (channel_count == 4)
                format = TextureFormat::RGBA8;
        }

        size_t byte_count = static_cast<size_t>(width) * static_cast<size_t>(height) * channel_count * channel_size;
        result.resize(byte_count);
        std::memcpy(result.data(), data, byte_count);

        stbi_image_free(data);
    }

    return std::make_pair(glm::uvec2(width, height), result);
}

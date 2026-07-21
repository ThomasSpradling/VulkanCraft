#include "AssetManager.h"
#include "Buffer.h"
#include "Core/Handle.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Platform/Graphics/Common.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanImage.h"
#include "GLTFModel.h"
#include "Texture.h"

#include <future>
#include <sstream>
#include <stb_image.h>

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

TextureHandle AssetManager::LoadTexture(const std::filesystem::path &path, TextureFormat format) {
    if (format != TextureFormat::RGBA8 && format != TextureFormat::RGBA8Srgb)
        throw std::runtime_error("STB texture loading only supports RGBA8 formats");

    int width = 0;
    int height = 0;
    int channel_count = 0;

    stbi_uc *loaded_pixels = stbi_load(path.string().c_str(), &width, &height, &channel_count, STBI_rgb_alpha);
    if (loaded_pixels == nullptr)
        throw std::runtime_error(std::format("Failed to load texture '{}': {}", path.string(), stbi_failure_reason()));

    size_t byte_count = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;

    Texture texture {
        .extent = glm::uvec3(static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1),
        .format = format,
        .pixels = std::vector<std::byte>(byte_count),
        .generate_mipmaps = true,
    };

    std::memcpy(texture.pixels.data(), loaded_pixels, byte_count);
    stbi_image_free(loaded_pixels);

    return CreateTexture(texture);
}

TextureHandle AssetManager::CreateTexture(const Texture &texture) {
    const auto get_texture_format = [](TextureFormat format) -> VkFormat {
        switch (format) {
            case TextureFormat::R8:
                return VK_FORMAT_R8_UNORM;
            case TextureFormat::RG8:
                return VK_FORMAT_R8G8_UNORM;
            case TextureFormat::RGBA8Srgb:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case TextureFormat::RGBA16Float:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case TextureFormat::RGBA32Float:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case TextureFormat::RGBA8:
            default:
                return VK_FORMAT_R8G8B8A8_UNORM;
        }
    };

    auto gpu_texture_builder = VulkanImage::ImageBuilder(m_device)
        .Image2D(texture.extent.x, texture.extent.y)
        .Format(get_texture_format(texture.format))
        .AddUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    if (texture.generate_mipmaps)
        gpu_texture_builder.MipMaps();

    auto gpu_texture = gpu_texture_builder.Build();
    gpu_texture->TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    gpu_texture->Upload(texture.pixels.data(), sizeof(std::byte) * texture.pixels.size());

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

    TextureId texture_id = m_bindless_table.AddTexture(gpu_texture);
    TextureRecord record {
        .texture_id = texture_id,
    };
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
    TextureId texture_id = m_textures.Get(texture).texture_id;
    VulkanImage &image = m_bindless_table.GetTexture(texture_id);
    return std::make_pair(texture_id, std::reference_wrapper(image));
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

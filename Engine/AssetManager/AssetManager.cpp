#include "AssetManager.h"
#include "Core/Handle.h"
#include "Platform/Graphics/CommandBuffer.h"
#include "Platform/Graphics/VulkanBuffer.h"
#include "Platform/Graphics/VulkanImage.h"
#include "GLTFModel.h"
#include "Texture.h"

AssetManager::AssetManager(const VulkanDevice &device, BindlessDescriptorTable &descriptor_table)
    : m_device(device)
    , m_bindless_table(descriptor_table)
{
    m_pbr_material_buffer = VulkanBuffer::BufferBuilder(device)
        .Size(sizeof(GPUMetallicRoughnessMaterial) * MaxPbrMaterials)
        .AddUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        .AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        .Build();

    m_basic_material_buffer = VulkanBuffer::BufferBuilder(device)
        .Size(sizeof(GPUBasicMaterial) * MaxBasicMaterials)
        .AddUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        .AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        .Build();

    GPUMetallicRoughnessMaterial default_pbr {};
    GPUBasicMaterial default_basic {};

    m_pbr_material_buffer->UploadAt(&default_pbr, 0);
    m_basic_material_buffer->UploadAt(&default_basic, 0);
}

AssetManager::~AssetManager() {
    m_basic_material_buffer.reset();
    m_pbr_material_buffer.reset();
}

GLTFHandle AssetManager::LoadGLTF(const std::filesystem::path &path) {
    auto current_model = std::make_unique<GLTFModel>(m_device, *this, path);
    return m_gltf_models.Add(current_model);
}

MeshHandle AssetManager::CreateMesh(const std::vector<MeshVertex> &vertices, const std::vector<uint32_t> &indices) {
    auto current_mesh = std::make_unique<Mesh>(m_device, vertices, indices);
    return m_meshes.Add(current_mesh);
}

const GLTFModel &AssetManager::GetGLTF(GLTFHandle gltf) {
    return *m_gltf_models.Get(gltf);
}

const Mesh &AssetManager::GetMesh(MeshHandle mesh) {
    return *m_meshes.Get(mesh);
}

MaterialHandle AssetManager::CreateMaterial(const MetallicRoughnessMaterial &material) {
    GPUMetallicRoughnessMaterial gpu_material {
        .base_color = material.albedo,
        .metallic = material.metallic,
        .roughness = material.roughness,
        .albedo_texture = GetTextureId(material.albedo_texture),
        .albedo_sampler = GetSamplerId(material.albedo_sampler),
        .albedo_texcoord = material.albedo_texcoord,
        .normal_texture = GetTextureId(material.normal_texture),
        .normal_sampler = GetSamplerId(material.normal_sampler),
        .normal_texcoord = material.normal_texcoord,
        .normal_texture_scale = material.normal_scale,
    };

    if (m_pbr_material_buffer_index + 1 < MaxPbrMaterials) {
        m_pbr_material_buffer_index++;
        m_pbr_material_buffer->UploadAt(&gpu_material, m_pbr_material_buffer_index);
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
        m_basic_material_buffer->UploadAt(&gpu_material, m_basic_material_buffer_index);
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

TextureHandle AssetManager::CreateTexture(const TextureData &texture) {
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
            default:
            case TextureFormat::RGBA8:
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

SamplerId AssetManager::GetSamplerId(SamplerHandle sampler) {
    if (!sampler)
        return DefaultSamplerId;

    return m_samplers.Get(sampler).sampler_id;
}

const VulkanBuffer &AssetManager::MaterialBuffer(MaterialType type) const {
    switch (type) {
        case MaterialType::MetallicRoughness:
            return *m_pbr_material_buffer;

        case MaterialType::Basic:
            return *m_basic_material_buffer;
    }

    std::unreachable();
}

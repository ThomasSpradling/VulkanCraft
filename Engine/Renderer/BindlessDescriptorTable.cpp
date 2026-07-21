#include "BindlessDescriptorTable.h"
#include "Platform/Graphics/CommandBuffer.h"
#include <iostream>

BindlessDescriptorTable::BindlessDescriptorTable(const VulkanDevice &device)
    : m_device(device)
{
    //// Descriptor Pool ////
    std::vector<DescriptorPoolRatios> ratios = {
        { .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .ratio = MaxTextures + MaxCubeTextures },
        { .type = VK_DESCRIPTOR_TYPE_SAMPLER, .ratio = MaxSamplers },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = MaxStorageImages },
        { .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .ratio = MaxAccelerationStructures },
    };
    m_descriptor_allocator = std::make_unique<DescriptorAllocator>(m_device, 1, ratios, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

    //// Descriptor Set Layout ////
    std::array<VkDescriptorSetLayoutBinding, 5> bindings {
        VkDescriptorSetLayoutBinding {
            .binding = static_cast<uint32_t>(DescriptorBinding::Textures_2D),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = MaxTextures,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = nullptr,
        },
        VkDescriptorSetLayoutBinding {
            .binding = static_cast<uint32_t>(DescriptorBinding::Textures_Cube),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = MaxCubeTextures,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = nullptr,
        },
        VkDescriptorSetLayoutBinding {
            .binding = static_cast<uint32_t>(DescriptorBinding::Samplers),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = MaxSamplers,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = nullptr,
        },
        VkDescriptorSetLayoutBinding {
            .binding = static_cast<uint32_t>(DescriptorBinding::StorageImages),
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = MaxStorageImages,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = nullptr,
        },
        VkDescriptorSetLayoutBinding {
            .binding = static_cast<uint32_t>(DescriptorBinding::AccelerationStructures),
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = MaxAccelerationStructures,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = nullptr,
        },
    };

    VkDescriptorBindingFlags image_binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
        | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    VkDescriptorBindingFlags acceleration_structure_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
        | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    std::array<VkDescriptorBindingFlags, 5> binding_flags {
        image_binding_flags,             // Textures
        image_binding_flags,             // Textures
        image_binding_flags,             // Samplers
        image_binding_flags,             // Storage images
        acceleration_structure_flags,    // Acceleration structures
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = static_cast<uint32_t>(binding_flags.size()),
        .pBindingFlags = binding_flags.data(),
    };

    VkDescriptorSetLayoutCreateInfo layout_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flags_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    VkDescriptorSetLayoutSupport support {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT,
    };

    vkGetDescriptorSetLayoutSupport(m_device.Device(), &layout_info, &support);
    Assert(support.supported == VK_TRUE, "Global descriptor set layout is not supported");

    VK_CHECK(vkCreateDescriptorSetLayout(m_device.Device(), &layout_info, nullptr, &m_global_descriptor_layout));

    //// Global Descriptor Set ////
    m_global_descriptor_set = m_descriptor_allocator->AllocateDescriptorSet(m_global_descriptor_layout);
    std::cout << "Created GPU resource manager.\n";

    //// Add Default Texture ////
    std::array<glm::u8vec4, 1> default_pixels {
        glm::u8vec4(180, 0, 0, 255)
    };

    auto default_texture = VulkanImage::ImageBuilder(m_device)
        .Image2D(1, 1)
        .Format(VK_FORMAT_R8G8B8A8_UNORM)
        .AddUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .Build();
    default_texture->TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    default_texture->Upload(default_pixels.data(), sizeof(glm::u8vec4) * default_pixels.size());

    m_device.ImmediateSubmit(QueueType::Graphics, [&](const CommandBuffer &cmd) {
        cmd.ImageMemoryBarrier(*default_texture)
            .DestAccess(VK_ACCESS_2_SHADER_SAMPLED_READ_BIT)
            .DestStage(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR)
            .TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .Execute();
    });
    
    m_textures[DefaultTextureId] = std::move(default_texture);
    WriteTexture(DefaultTextureId, *m_textures[DefaultTextureId]);

    //// Add Default Sampler ////
    
    VkSamplerCreateInfo sampler_create_info {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,

        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,

        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,

        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,

        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,

        .minLod = 0.0f,
        .maxLod = 0.0f,

        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    
    VkSampler sampler;
    vkCreateSampler(m_device.Device(), &sampler_create_info, nullptr, &sampler);

    m_samplers[DefaultSamplerId] = sampler;
    WriteSampler(0, m_samplers[DefaultSamplerId]);
}

BindlessDescriptorTable::~BindlessDescriptorTable() {
    std::cout << "Destroying GPU resource manager.\n";
    for (auto &texture : m_textures) {
        texture.reset();
    }

    for (auto sampler : m_samplers) {
        vkDestroySampler(m_device.Device(), sampler, nullptr);
    }

    for (auto &image : m_storage_images) {
        image.reset();
    }

    for (auto acceleration_structure : m_acceleration_structures) {
        vkDestroyAccelerationStructureKHR(m_device.Device(), acceleration_structure, nullptr);
    }
    
    m_descriptor_allocator.reset();

    if (m_global_descriptor_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device.Device(), m_global_descriptor_layout, nullptr);
        m_global_descriptor_layout = VK_NULL_HANDLE;
    }
}

TextureId BindlessDescriptorTable::AddTexture(std::unique_ptr<VulkanImage> &image) {
    TextureId id = m_texture_ids.Acquire();
    WriteTexture(id, *image);
    m_textures[id] = std::move(image);
    return id;
}

SamplerId BindlessDescriptorTable::AddSampler(VkSampler sampler) {
    SamplerId id = m_sampler_ids.Acquire();
    WriteSampler(id, sampler);
    m_samplers[id] = sampler;
    return id;
}

StorageImageId BindlessDescriptorTable::AddStorageImage(std::unique_ptr<VulkanImage> &image) {
    StorageImageId id = m_storage_image_ids.Acquire();
    WriteStorageImage(id, *image);
    m_storage_images[id] = std::move(image);
    return id;
}

AccelerationStructureId BindlessDescriptorTable::AddAccelerationStructure(VkAccelerationStructureKHR acceleration_structure) {
    AccelerationStructureId id = m_acceleration_structure_ids.Acquire();
    WriteAccelerationStructure(id, acceleration_structure);
    m_acceleration_structures[id] = acceleration_structure;
    return id;
}

VulkanImage &BindlessDescriptorTable::GetTexture(TextureId id) {
    Assert(id < MaxTextures && m_textures[id], "Invalid texture ID");
    return *m_textures[id];
}

VkSampler BindlessDescriptorTable::GetSampler(SamplerId id) {
    Assert(id < MaxSamplers && m_samplers[id], "Invalid sampler ID");
    return m_samplers[id];
}

VulkanImage &BindlessDescriptorTable::GetStorageImage(StorageImageId id) {
    Assert(id < MaxStorageImages && m_storage_images[id], "Invalid storage image ID");
    return *m_storage_images[id];
}

VkAccelerationStructureKHR BindlessDescriptorTable::GetAccelerationStructure(AccelerationStructureId id) {
    Assert(id < MaxAccelerationStructures && m_acceleration_structures[id] != VK_NULL_HANDLE, "Invalid acceleration structure ID");
    return m_acceleration_structures[id];
}

void BindlessDescriptorTable::RemoveTexture(TextureId id) {
    Assert(id > DefaultTextureId && id < MaxTextures, "Invalid texture ID");
    Assert(m_textures[id], "Texture slot is empty");

    WriteTexture(id, *m_textures[DefaultTextureId]);
    m_textures[id].reset();
    m_texture_ids.Release(id);
}

void BindlessDescriptorTable::RemoveSampler(SamplerId id) {
    Assert(id > DefaultSamplerId && id < MaxSamplers, "Invalid sampler ID");
    Assert(m_samplers[id] != VK_NULL_HANDLE, "Sampler slot is empty");

    WriteSampler(id, m_samplers[DefaultSamplerId]);
    vkDestroySampler(m_device.Device(), m_samplers[id], nullptr);
    m_samplers[id] = VK_NULL_HANDLE;
    m_sampler_ids.Release(id);
}

void BindlessDescriptorTable::ReplaceTexture(TextureId id, std::unique_ptr<VulkanImage> image) {
    Assert(id > DefaultTextureId && id < MaxTextures, "Invalid texture ID");
    Assert(m_textures[id], "Texture slot is empty");
    Assert(image, "Cannot replace a texture with null");

    WriteTexture(id, *image);
    m_textures[id] = std::move(image);
}

void BindlessDescriptorTable::ReplaceSampler(SamplerId id, VkSampler sampler) {
    Assert(id > DefaultSamplerId && id < MaxSamplers, "Invalid sampler ID");
    Assert(m_samplers[id] != VK_NULL_HANDLE, "Sampler slot is empty");
    Assert(sampler != VK_NULL_HANDLE, "Cannot replace a sampler with null");

    WriteSampler(id, sampler);
    vkDestroySampler(m_device.Device(), m_samplers[id], nullptr);
    m_samplers[id] = sampler;
}

void BindlessDescriptorTable::WriteTexture(TextureId id, const VulkanImage &image) {
    Assert(id < MaxTextures, "Invalid texture ID!");
    Assert(image.Layout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "Cannot write to texture without SHADER_READ_ONLY layout!");
    
    VkDescriptorImageInfo image_info {
        .sampler = VK_NULL_HANDLE,
        .imageView = image.View(),
        .imageLayout = image.Layout(),
    };

    VkWriteDescriptorSet write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_global_descriptor_set,
        .dstBinding = static_cast<uint32_t>(DescriptorBinding::Textures_2D),
        .dstArrayElement = id,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &image_info,
    };

    vkUpdateDescriptorSets(m_device.Device(), 1, &write, 0, nullptr);
}

void BindlessDescriptorTable::WriteSampler(SamplerId id, VkSampler sampler) {
    Assert(id < MaxSamplers, "Invalid texture ID!");
    Assert(sampler, "Mut have a valid sampler!");
    
    VkDescriptorImageInfo image_info {
        .sampler = sampler,
        .imageView = VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkWriteDescriptorSet write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_global_descriptor_set,
        .dstBinding = static_cast<uint32_t>(DescriptorBinding::Samplers),
        .dstArrayElement = id,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &image_info,
    };

    vkUpdateDescriptorSets(m_device.Device(), 1, &write, 0, nullptr);
}

void BindlessDescriptorTable::WriteStorageImage(StorageImageId id, const VulkanImage &image) {
    Assert(id < MaxStorageImages, "Invalid storage image ID!");
    Assert(image.Layout() == VK_IMAGE_LAYOUT_GENERAL, "Cannot write to storage image without GENERAL layout!");
    Assert(image.Usage() & VK_IMAGE_USAGE_STORAGE_BIT, "Cannot write to storage image using non-storage image.");

    VkDescriptorImageInfo image_info {
        .sampler = VK_NULL_HANDLE,
        .imageView = image.View(),
        .imageLayout = image.Layout(),
    };

    VkWriteDescriptorSet write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_global_descriptor_set,
        .dstBinding = static_cast<uint32_t>(DescriptorBinding::StorageImages),
        .dstArrayElement = id,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &image_info,
    };

    vkUpdateDescriptorSets(m_device.Device(), 1, &write, 0, nullptr);
}


void BindlessDescriptorTable::WriteAccelerationStructure(AccelerationStructureId id, VkAccelerationStructureKHR acceleration_structure) {
    Assert(id < MaxAccelerationStructures, "Invalid storage image ID!");
    
    VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_info {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &acceleration_structure,
    };

    VkWriteDescriptorSet write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &acceleration_structure_info,
        .dstSet = m_global_descriptor_set,
        .dstBinding = static_cast<uint32_t>(DescriptorBinding::AccelerationStructures),
        .dstArrayElement = id,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };

    vkUpdateDescriptorSets(m_device.Device(), 1, &write, 0, nullptr);
}

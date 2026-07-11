#include "GPUResourceManager.h"
#include <iostream>

GPUResourceManager::GPUResourceManager(const VulkanDevice &device)
    : m_device(device)
{
    //// Descriptor Pool ////
    std::vector<DescriptorPoolRatios> ratios = {
        { .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .ratio = MaxTextures },
        { .type = VK_DESCRIPTOR_TYPE_SAMPLER, .ratio = MaxSamplers },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = MaxStorageImages },
        { .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .ratio = MaxAccelerationStructures },
    };
    m_descriptor_allocator = std::make_unique<DescriptorAllocator>(m_device, 1, ratios, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

    //// Descriptor Set Layout ////
    std::array<VkDescriptorSetLayoutBinding, 4> bindings {
        VkDescriptorSetLayoutBinding {
            .binding = static_cast<uint32_t>(DescriptorBinding::Textures),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = MaxTextures,
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


    std::array<VkDescriptorBindingFlags, 4> binding_flags {
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
}

GPUResourceManager::~GPUResourceManager() {
    std::cout << "Destroying GPU resource manager.\n";
    m_descriptor_allocator.reset();

    if (m_global_descriptor_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device.Device(), m_global_descriptor_layout, nullptr);
        m_global_descriptor_layout = VK_NULL_HANDLE;
    }
}

TextureId GPUResourceManager::AddTexture(const VulkanImage &image) {
    TextureId id = m_textures.Acquire();
    WriteTexture(id, image);
    return id;
}

SamplerId GPUResourceManager::AddSampler(VkSampler sampler) {
    SamplerId id = m_samplers.Acquire();
    WriteSampler(id, sampler);
    return id;
}

StorageImageId GPUResourceManager::AddStorageImage(const VulkanImage &image) {
    StorageImageId id = m_storage_images.Acquire();
    WriteStorageImage(id, image);
    return id;
}

AccelerationStructureId GPUResourceManager::AddAccelerationStructure(VkAccelerationStructureKHR acceleration_structure) {
    AccelerationStructureId id = m_acceleration_structures.Acquire();
    WriteAccelerationStructure(id, acceleration_structure);
    return id;
}

void GPUResourceManager::WriteTexture(TextureId id, const VulkanImage &image) {
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
        .dstBinding = static_cast<uint32_t>(DescriptorBinding::Textures),
        .dstArrayElement = id,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &image_info,
    };

    vkUpdateDescriptorSets(m_device.Device(), 1, &write, 0, nullptr);
}

void GPUResourceManager::WriteSampler(SamplerId id, VkSampler sampler) {
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

void GPUResourceManager::WriteStorageImage(StorageImageId id, const VulkanImage &image) {
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


void GPUResourceManager::WriteAccelerationStructure(AccelerationStructureId id, VkAccelerationStructureKHR acceleration_structure) {
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

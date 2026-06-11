#include "DescriptorAllocator.h"
#include "utils.h"
#include <algorithm>
#include <cmath>
#include <volk.h>

void DescriptorAllocator::Init(uint32_t max_sets, const std::vector<DescriptorPoolRatios> &ratios) {
    m_sets_per_pool = max_sets;
    VkDescriptorPool pool = CreateDescriptorPool();
    m_sets_per_pool = max_sets * 1.5f;
    m_ready_pools.push_back(pool);
}
    

void DescriptorAllocator::ClearDescriptorSets() {
    for (auto p : m_ready_pools)
        vkResetDescriptorPool(m_device, p, 0);

    for (auto p : m_full_pools) {
        vkResetDescriptorPool(m_device, p, 0);
        m_ready_pools.push_back(p);
    }
    m_full_pools.clear();
}

void DescriptorAllocator::Destroy() {
    for (auto p : m_ready_pools)
        vkDestroyDescriptorPool(m_device, p, nullptr);
    m_ready_pools.clear();

    for (auto p : m_full_pools)
        vkDestroyDescriptorPool(m_device, p, nullptr);
    m_full_pools.clear();
}

VkDescriptorSet DescriptorAllocator::AllocateDescriptorSet(VkDescriptorSetLayout layout) {
    VkDescriptorPool pool = GetDesriptorPool();
    
    VkDescriptorSet descriptor_set;
    VkDescriptorSetAllocateInfo allocate_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };
    VkResult result = vkAllocateDescriptorSets(m_device, &allocate_info, &descriptor_set);
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        m_full_pools.push_back(pool);
        pool = GetDesriptorPool();
        allocate_info.descriptorPool = pool;
        VK_CHECK(vkAllocateDescriptorSets(m_device, &allocate_info, &descriptor_set));
    }
    m_ready_pools.push_back(pool);
    return descriptor_set;
}

VkDescriptorPool DescriptorAllocator::CreateDescriptorPool() {
    VkDescriptorPool pool;

    std::vector<VkDescriptorPoolSize> sizes(m_ratios.size());
    for (uint32_t i = 0; i < m_ratios.size(); ++i) {
        sizes[i].type = m_ratios[i].type;
        sizes[i].descriptorCount = static_cast<uint32_t>(std::ceil(m_sets_per_pool * m_ratios[i].ratio));
    }
    
    VkDescriptorPoolCreateInfo descriptor_pool_create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .maxSets = m_sets_per_pool,
        .poolSizeCount = static_cast<uint32_t>(sizes.size()),
        .pPoolSizes = sizes.data(),
    };
    VK_CHECK(vkCreateDescriptorPool(m_device, &descriptor_pool_create_info, nullptr, &pool));
    return pool;
}

VkDescriptorPool DescriptorAllocator::GetDesriptorPool() {
    if (m_ready_pools.size() != 0) {
        VkDescriptorPool pool = m_ready_pools.back();
        m_ready_pools.pop_back();
        return pool;
    } else {
        VkDescriptorPool pool = CreateDescriptorPool();
        
        // increase pool size for next pool needed to be allocated
        m_sets_per_pool = std::max(static_cast<uint32_t>(std::ceil(m_sets_per_pool * 1.5)), MAX_SETS_PER_POOL);
        return pool;
    }
}

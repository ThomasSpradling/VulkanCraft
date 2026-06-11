#pragma once

#include <vector>
#include <volk.h>

struct DescriptorPoolRatios {
    VkDescriptorType type;
    float ratio;
};

class DescriptorAllocator {
public:
    DescriptorAllocator(VkDevice device) : m_device(device) {}
    void Init(uint32_t max_sets, const std::vector<DescriptorPoolRatios> &ratios);
    void ClearDescriptorSets();
    void Destroy();

    VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);
private:
    VkDevice m_device;
    // VkDescriptorPool m_descriptor_pool;

    std::vector<DescriptorPoolRatios> m_ratios;
    uint32_t m_sets_per_pool;
    const uint32_t MAX_SETS_PER_POOL = 4096u;

    std::vector<VkDescriptorPool> m_full_pools;
    std::vector<VkDescriptorPool> m_ready_pools;
private:
    VkDescriptorPool CreateDescriptorPool();
    VkDescriptorPool GetDesriptorPool();
};

#pragma once

#include "VulkanDevice.h"
#include <vector>
#include <volk.h>

struct DescriptorPoolRatios {
    VkDescriptorType type;
    float ratio;
};

class DescriptorAllocator : public NonCopyable, public NonMovable {
public:
    DescriptorAllocator(const VulkanDevice &device, uint32_t max_sets, const std::vector<DescriptorPoolRatios> &ratios);
    ~DescriptorAllocator();
    
    VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);
    void ClearDescriptorSets();
private:
    const VulkanDevice &m_device;

    std::vector<DescriptorPoolRatios> m_ratios;
    uint32_t m_sets_per_pool;
    const uint32_t MAX_SETS_PER_POOL = 4096u;

    std::vector<VkDescriptorPool> m_full_pools;
    std::vector<VkDescriptorPool> m_ready_pools;
private:
    VkDescriptorPool CreateDescriptorPool();
    VkDescriptorPool GetDesriptorPool();
};

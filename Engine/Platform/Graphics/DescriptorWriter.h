#pragma once

#include "VulkanDevice.h"
#include <vector>
#include <volk.h>

class DescriptorWriter {
public:
    DescriptorWriter(const VulkanDevice &device) : m_device(device) {}

    DescriptorWriter &WriteImage(uint32_t binding, VkDescriptorType type, const VkDescriptorImageInfo &info);
    DescriptorWriter &WriteBuffer(uint32_t binding, VkDescriptorType type, const VkDescriptorBufferInfo &buffer);
    DescriptorWriter &Clear();
    void Write(VkDescriptorSet set);
private:
    const VulkanDevice &m_device;
    std::vector<VkWriteDescriptorSet> m_writes;
};

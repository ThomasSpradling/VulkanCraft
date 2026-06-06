#pragma once

#include <vector>
#include <volk.h>

class DescriptorLayoutBuilder {
public:
    DescriptorLayoutBuilder(VkDevice device) : m_device(device) {}
    
    DescriptorLayoutBuilder &AddBinding(uint32_t binding, VkDescriptorType type);
    DescriptorLayoutBuilder &Clear();
    VkDescriptorSetLayout Build(VkShaderStageFlags stages, VkDescriptorSetLayoutCreateFlags flags = 0, void *p_next = nullptr);
private:
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
    VkDevice m_device; 
};

#include "descriptor_layout_builder.h"
#include "utils.h"

DescriptorLayoutBuilder &DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type) {
    m_bindings.push_back({
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
    });
    return *this;
}

DescriptorLayoutBuilder &DescriptorLayoutBuilder::Clear() {
    m_bindings.clear();
    return *this;
}

VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VkShaderStageFlags stages, VkDescriptorSetLayoutCreateFlags flags, void *p_next) {
    for (auto &binding : m_bindings) {
        binding.stageFlags |= stages;
    }

    VkDescriptorSetLayout set_layout;
    
    VkDescriptorSetLayoutCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = p_next,
        .flags = flags,
        .bindingCount = static_cast<uint32_t>(m_bindings.size()),
        .pBindings = m_bindings.data(),
    };
    VK_CHECK(vkCreateDescriptorSetLayout(m_device, &create_info, nullptr, &set_layout));
    return set_layout;
}

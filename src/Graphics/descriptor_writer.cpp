#include "descriptor_writer.h"
#include "utils.h"

DescriptorWriter &DescriptorWriter::WriteImage(uint32_t binding, VkDescriptorType type, const VkDescriptorImageInfo &info) {
    VkWriteDescriptorSet write_descriptor_set {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = &info,
    };
    m_writes.push_back(write_descriptor_set);
    return *this;
}

DescriptorWriter &DescriptorWriter::WriteBuffer(uint32_t binding, VkDescriptorType type, const VkDescriptorBufferInfo &info) {
    VkWriteDescriptorSet write_descriptor_set {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &info,
    };
    m_writes.push_back(write_descriptor_set);
    return *this;
}

DescriptorWriter &DescriptorWriter::Clear() {
    m_writes.clear();
    return *this;
}

void DescriptorWriter::Write(VkDescriptorSet set) {
    for (VkWriteDescriptorSet &write : m_writes)
        write.dstSet = set;

    vkUpdateDescriptorSets(m_device, m_writes.size(), m_writes.data(), 0, nullptr);
}
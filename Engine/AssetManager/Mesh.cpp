#include "Mesh.h"

Mesh::Mesh(const VulkanDevice &device, const std::vector<MeshVertex> &vertices, const std::vector<uint32_t> &indices) {
    m_vertex_buffer = VulkanBuffer::BufferBuilder(device)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        .AddUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        .Size(vertices.size() * sizeof(MeshVertex))
        .Build();

    m_vertex_buffer->Upload(vertices);

    m_index_buffer = VulkanBuffer::BufferBuilder(device)
        .AddUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        .AddUsage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
        .Size(indices.size() * sizeof(uint32_t))
        .Build();

    m_index_buffer->Upload(indices);
    m_index_count = static_cast<uint32_t>(indices.size());
}

Mesh::~Mesh() {
    m_vertex_buffer.reset();
    m_index_buffer.reset();
}

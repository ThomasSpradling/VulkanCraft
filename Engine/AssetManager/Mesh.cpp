#include "Mesh.h"
#include "AssetManager.h"
#include "Buffer.h"

Mesh::Mesh(AssetManager &asset_manager, const std::vector<MeshVertex> &vertices, const std::vector<uint32_t> &indices, const std::string &debug_name)
    : m_assets(asset_manager)
{
    m_vertex_buffer = asset_manager.CreateBuffer(GPUBufferData {
        .usage = BufferUsageBits::Storage,
        .size = vertices.size() * sizeof(MeshVertex),
        .data = vertices.data(),
        .debug_name = std::format("Mesh ({}) Vertex Buffer", debug_name),
    });

    m_index_buffer = asset_manager.CreateBuffer(GPUBufferData {
        .usage = BufferUsageBits::Index,
        .size = indices.size() * sizeof(uint32_t),
        .data = indices.data(),
        .debug_name = std::format("Mesh ({}) Index Buffer", debug_name),
    });
    
    m_index_count = static_cast<uint32_t>(indices.size());
}

Mesh::~Mesh() {
    m_assets.DestroyBuffer(m_vertex_buffer);
    m_assets.DestroyBuffer(m_index_buffer);
}

const VulkanBuffer &Mesh::VertexBuffer() const {
    return m_assets.GetBuffer(m_vertex_buffer);
}

const VulkanBuffer &Mesh::IndexBuffer() const {
    return m_assets.GetBuffer(m_index_buffer);
}

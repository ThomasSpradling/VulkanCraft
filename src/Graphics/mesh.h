#pragma once

#include <vector>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

// The UV being split ensures data ends on 16-byte alignments
struct MeshVertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

//////////////////////////
//// --- GPU Data --- ////
//////////////////////////

struct GPUMesh {
    VkBuffer vertex_buffer;
    VmaAllocation vertex_buffer_alloc;

    VkBuffer index_buffer;
    VmaAllocation index_buffer_alloc;

    VkDeviceAddress device_address;
};

// class GltfMeshAsset {
// public:
// };

//////////////////////////////////////
//// --- CPU Data and Methods --- ////
//////////////////////////////////////

#pragma once

#include <vector>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 color;
    glm::vec2 uv;
};

//////////////////////////
//// --- GPU Data --- ////
//////////////////////////

struct GPUMesh {
    VkBuffer vertex_buffer;
    VmaAllocation vertex_buffer_alloc;

    VkBuffer index_buffer;
    VmaAllocation index_buffer_alloc;
};

// class GltfMeshAsset {
// public:
// };

//////////////////////////////////////
//// --- CPU Data and Methods --- ////
//////////////////////////////////////

#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

//// Image and Texture Data ////
struct GPUImage {
    VkImage image;
    VkImageView image_view;
    VmaAllocation allocation;
    VkExtent3D extent;
    VkFormat format;
};

struct GPUTexture {
    GPUImage image;
    VkSampler sampler;
    VkDescriptorSet descriptor_set;
};

//// Buffer Data ////
struct GPUBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
};

//// Mesh Data ////
// The UV being split ensures data ends on 16-byte alignments
struct MeshVertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct GPUMesh {
    uint32_t index_count = 0;

    VkBuffer vertex_buffer;
    VmaAllocation vertex_buffer_alloc;

    VkBuffer index_buffer;
    VmaAllocation index_buffer_alloc;

    VkDeviceAddress device_address;
};

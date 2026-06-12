#pragma once

#include <cstdint>
#include <vector>
#include <volk.h>
#include <glm/glm.hpp>

enum class MaterialPass {
    Opaque,
    Transparent,
};

// minimal data required to bind and execute a VkDrawIndexed call
struct RenderObject {
    uint32_t index_count;
    uint32_t start_index;
    VkBuffer index_buffer;

    glm::mat4 transform;
    VkDeviceAddress vertex_buffer;

    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSet descriptor_set;
};

struct DrawContext {
    std::vector<RenderObject> opaque_objects;
    std::vector<RenderObject> transparent_objects;
};

/**
 * @brief Any class that implements this interface inhabits the Draw() method, which should directly update the draw context.
 * 
 */
class IRenderable {
public:
    virtual void Draw(DrawContext &context, const glm::mat4 &transform) = 0;
};

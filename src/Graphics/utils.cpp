#include "utils.h"

//////////////////////////////////
///  ---- VULKAN COMMANDS ---- ///
//////////////////////////////////

void TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageSubresourceRange subresource_range, VkImageLayout old_layout, VkImageLayout new_layout) {   
    VkImageMemoryBarrier2 draw_image_barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .image = image,
        .subresourceRange = subresource_range,
    };
    
    VkDependencyInfo dependency_info {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &draw_image_barrier,
    };

    vkCmdPipelineBarrier2(cmd, &dependency_info);
}
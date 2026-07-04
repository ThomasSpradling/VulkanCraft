// #pragma once

// #include <vector>

// #include "../Platform/Graphics/VulkanDevice.h"

// class GPUMemoryProfiler {
// public:
//     GPUMemoryProfiler(const VulkanDevice &device)
//         : m_device(device) {}

//     void Record();
// private:
//     struct MemoryStatistic {
//         VkDeviceSize vma_usage = 0;  // Usage from VkDeviceMemory blocks via VMA
//         VkDeviceSize heap_usage = 0; // Total estimated GPU usaged
//         VkDeviceSize budget = 0;
//     };
// private:
//     const VulkanDevice &m_device;

//     std::vector<MemoryStatistic> m_data;
// };
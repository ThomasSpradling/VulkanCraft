// #include "GPUMemoryProfiler.h"

// void GPUMemoryProfiler::Record() {
//     VmaBudget vma_statistic;
//     vmaGetHeapBudgets(m_device.Allocator(), &vma_statistic);

//     MemoryStatistic statistic {
//         .vma_usage = vma_statistic.statistics.blockBytes,
//         .heap_usage = vma_statistic.usage,
//         .budget = vma_statistic.budget, 
//     };
//     m_data.push_back(statistic);
// }

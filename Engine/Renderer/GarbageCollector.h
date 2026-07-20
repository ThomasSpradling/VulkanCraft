#pragma once

// Gargbage collector for GPU per-frame resources
#include <cstdint>
#include <future>
class GarbageCollector {
public:
    GarbageCollector(uint32_t frames_in_flight);

    void SetCurrentFrame(uint32_t frame);

    void Enqueue(std::packaged_task<void()> task);
    void Collect(uint32_t current_frame);
    void CollectAll();
private:
    uint32_t m_current_frame = 0;
    std::vector<std::vector<std::packaged_task<void()>>> m_queues;
};

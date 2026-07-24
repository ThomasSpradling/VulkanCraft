#include "GarbageCollector.h"
#include "Core/Core.h"

GarbageCollector::GarbageCollector(uint32_t frames_in_flight) {
    m_queues.resize(frames_in_flight);
}

void GarbageCollector::SetCurrentFrame(uint32_t frame) {
    m_current_frame = frame;
}

void GarbageCollector::Enqueue(std::packaged_task<void()> task) {
    m_queues[m_current_frame].push_back(std::move(task));
}

void GarbageCollector::Collect(uint32_t current_frame) {
    ENGINE_PROFILER_FUNCTION();

    auto &queue = m_queues[current_frame];
    for (auto &task : queue) {
        task();
    }
    queue.clear();
}

void GarbageCollector::CollectAll() {
    for (uint32_t i = 0; i < m_queues.size(); ++i) {
        Collect(i);
    }
}

#pragma once

#include "Core/errors.h"
#include <vector>
template <size_t Capacity>
struct IndexFreeList {
    std::vector<uint32_t> data;

    IndexFreeList() {
        static_assert(Capacity > 1);
        data.reserve(Capacity - 1);

        for (uint32_t i = Capacity - 1; i > 0; --i)
            data.push_back(i);
    }

    uint32_t Acquire() {
        ENGINE_ASSERT(!data.empty(), "Descriptor table is full!");

        uint32_t id = data.back();
        data.pop_back();
        return id;
    }
    
    void Release(uint32_t id) {
        ENGINE_ASSERT(id > 0 && id < Capacity, "Invalid id");

        data.push_back(id);
        return;
    }
};

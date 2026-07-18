#pragma once

#include "Core/Handle.h"
#include <vector>

template<typename T, typename Tag>
struct ResourcePool {
    std::vector<T> data;
    std::vector<uint32_t> generations;
    std::vector<uint32_t> available_indices;

    ResourcePool() {
        data.resize(1'000);
        generations.resize(1'000, 0);
    }

    Handle<Tag> Add(T &record) {
        uint32_t index;

        if (!available_indices.empty()) {
            index = available_indices.back();
            available_indices.pop_back();

            data[index] = std::move(record);
        } else {
            index = static_cast<uint32_t>(data.size());
            data.push_back(std::move(record));
            generations.push_back(0);
        }

        Handle<Tag> handle(index, generations[index]);
        return handle;
    }
    
    T &Get(Handle<Tag> handle) {
        Assert(handle.Index() < data.size(), "Cannot get mesh thats out of range!");
        Assert(handle.Index() < generations.size(), "Cannot get mesh thats out of range!");
        Assert(handle.Generation() == generations[handle.Index()], "Mismatching generations for meshes!");

        return data[handle.Index()];
    }

    void Remove(Handle<Tag> handle) {
        if (handle.Index() >= data.size() || handle.Index() >= generations.size() || handle.m_generation != generations[handle.Index()]) {
            return;
        }

        data[handle.Index()] = T{};
        ++generations[handle.Index()];
        available_indices.push_back(handle.Index());
    }
};

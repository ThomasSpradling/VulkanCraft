#pragma once

#include "Core/Handle.h"
#include <vector>

template<typename T, typename Tag>
struct ResourcePool {
    std::vector<T> data;
    std::vector<uint32_t> generations;
    std::vector<uint32_t> available_indices;

    uint32_t m_num_objects = 0;

    ResourcePool() {
        data.reserve(1'000);
        generations.reserve(1'000);
    }

    Handle<Tag> Add(T &record) {
        uint32_t index;

        if (!available_indices.empty()) {
            index = available_indices.back();
            available_indices.pop_back();

            Assert(index < data.size(), "Used invalid index in resource pool!");
            data[index] = std::move(record);
        } else {
            index = static_cast<uint32_t>(data.size());
            data.push_back(std::move(record));
            generations.push_back(0);
        }
        ++m_num_objects;

        Handle<Tag> handle(index, generations[index]);
        return handle;
    }
    
    T &Get(Handle<Tag> handle) {
        Assert(handle.Index() < data.size(), "Cannot get resource thats out of range!");
        Assert(handle.Index() < generations.size(), "Cannot get resource thats out of range!");
        Assert(handle.Generation() == generations[handle.Index()], "Mismatching generations for resources!");

        return data[handle.Index()];
    }

    const T &Get(Handle<Tag> handle) const {
        Assert(handle.Index() < data.size(), "Cannot get resource thats out of range!");
        Assert(handle.Index() < generations.size(), "Cannot get resource thats out of range!");
        Assert(handle.Generation() == generations[handle.Index()], "Mismatching generations for resources!");

        return data[handle.Index()];
    }

    T TakeOwnership(Handle<Tag> handle) {
        const uint32_t index = handle.Index();
        Assert(index < data.size(), "Resource index out of range!");
        Assert(index < generations.size(), "Resource generation index out of range!");
        Assert(handle.Generation() == generations[index], "Resource generation mismatch!");

        T resource = std::move(data[index]);
        data[index] = T{};

        ++generations[index];
        available_indices.push_back(index);

        Assert(m_num_objects > 0, "Resource pool count underflow!");
        --m_num_objects;

        return resource;
    }

    void Clear() {
        data.clear();
        generations.clear();
        available_indices.clear();
        m_num_objects = 0;
    }

    uint32_t Count() const {
        return m_num_objects;
    }
};

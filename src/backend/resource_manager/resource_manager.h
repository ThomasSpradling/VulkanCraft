#pragma once



#include <algorithm>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

#include "resource.h"
// #include "resource_handle.h"

template <typename T>
class ResourceHandle;

class ResourceManager {
public:
    template <typename T>
    ResourceHandle<T> Load(const std::string &id) {
        static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

        // Check if we have this resource already
        auto type_it = m_resources.find(std::type_index(typeid(T)));
        if (type_it == m_resources.end()) {
            m_resources.insert({std::type_index(typeid(T)), {}});
        }

        auto &type_resources = m_resources[std::type_index(typeid(T))];
        auto it = type_resources.find(id);

        if (it != type_resources.end()) {
            it->second.reference_count++;
            return ResourceHandle<T>(id, this);
        }

        // If not, then create it
        auto resource = std::make_shared<T>(id);
        if (!resource->LoadResource()) {
            return ResourceHandle<T>();
        }

        type_resources[id] = {
            .reference_count = 1,
            .resource = resource
        };
        return ResourceHandle<T>(id, this);
    }

    template <typename T>
    T *GetResource(const std::string &id) {
        auto &type_resources = m_resources[std::type_index(typeid(T))];
        auto it = type_resources.find(id);

        if (it != type_resources.end()) {
            return static_cast<T *>(it->second.resource.get());
        }
        return nullptr;
    }

    template <typename T>
    T *HasResource(const std::string &id) {
        auto &type_resources = m_resources[std::type_index(typeid(T))];
        auto it = type_resources.find(id);

        return it != type_resources.end();
    }

    template <typename T>
    void ReleaseResource(const std::string &id) {
        auto &type_resources = m_resources[std::type_index(typeid(T))];
        auto it = type_resources.find(id);

        it->second.reference_count--;
        if (it->second.reference_count <= 0) {
            type_resources.erase(it);
        }
    }

    void UnloadAll() {
        for (auto &[type, type_resources] : m_resources) {
            for (auto &[id, resource] : type_resources) {
                resource.resource->UnloadResource();
            }
            type_resources.clear();
        }
    }
private:
    struct CountedResource {
        int reference_count = 0;
        std::shared_ptr<Resource> resource;
    };
private:
    std::unordered_map<std::type_index, 
                       std::unordered_map<std::string, CountedResource>> m_resources = {};
};

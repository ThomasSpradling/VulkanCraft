#pragma once

#include "resource_manager.h"
#include <string>

template<typename T>
class ResourceHandle {
public:
    ResourceHandle() : m_resource_manager(nullptr) {}
    ResourceHandle(const std::string &id, ResourceManager *manager)
        : m_resource_id(id), m_resource_manager(manager) {}

    T *Get() const {
        if (!m_resource_manager) return nullptr;
        return m_resource_manager->GetResource<T>(m_resource_id);
    }

    bool IsValid() const {
        return m_resource_manager && m_resource_manager->HasResource<T>(m_resource_id);
    }

    const std::string &GetId() const {
        return m_resource_id;
    }
private:
    std::string m_resource_id;
    ResourceManager *m_resource_manager;
};

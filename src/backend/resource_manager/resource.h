#pragma once

/**
 * @brief Resource base class
 * 
 */
#include <string>
class Resource {
public:
    Resource(const std::string &id) : m_resource_id(id) {}
    virtual ~Resource() = default;

    const std::string& GetId() const { return m_resource_id; }
    bool IsLoaded() const { return m_loaded; }

    bool LoadResource() {
        m_loaded = Load();
        return m_loaded;
    }
    void UnloadResource() {
        Unload();
        m_loaded = false;
    }

private:
    std::string m_resource_id;
    bool m_loaded = false;

protected:
    virtual bool Load() = 0;
    virtual void Unload() = 0;
};

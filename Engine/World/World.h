#pragma once

#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include "Entity.h"
#include "DefaultComponents.h"
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include "Core/errors.h"

enum class TransformMethod : uint8_t {
    Local,
    Global,
};

class AssetManager;
class Application;
class World : public NonCopyable, public NonMovable {
    friend Application;
public:
    World(AssetManager &manager, uint32_t initial_entity_capacity = 100'000);

    // Builds GLTF as a prefab into this scene graph
    Entity BuildGLTF(GLTFHandle handle);

    // Note: Every entity is automatically created with a Transform component
    Entity CreateEntity(std::string_view name = "", const Transform &transform = {});

    void DestroyEntity(Entity entity, TransformMethod transform_method = TransformMethod::Local);
    void DestroyEntityTree(Entity root);

    bool IsValid(Entity entity) const;
    bool IsAlive(Entity entity) const;

    std::string_view Name(Entity entity) const;
    void SetName(Entity entity, std::string_view name);

    // `transform_method` defines wheather on creation of this relationship, if the child's transform
    // should be assumed as relative to parent or remain relative to world.
    // Note: Either way, the child will still always transform with the parent after initialization
    void AttachParent(Entity child, Entity parent, TransformMethod transform_method = TransformMethod::Local);
    void DetachParent(Entity child, TransformMethod transform_method = TransformMethod::Local);

    Entity Parent(Entity entity) const;
    std::span<const Entity> Children(Entity entity) const;
    Entity Root(Entity entity) const;

    template<typename Component, typename... Args>
    Component &Add(Entity entity, Args &&...args) {
        Assert(IsAlive(entity), "Cannot add a component to a non-alive entity!");

        auto type_id = std::type_index(typeid(Component));
        if (!m_component_storage.contains(type_id)) {
            m_component_storage[type_id] = std::make_unique<ComponentStorage<Component>>();
        }

        auto *component_storage = static_cast<ComponentStorage<Component> *>(m_component_storage[type_id].get());
        return component_storage->Add(entity, std::forward<Args>(args)...);
    }

    template<typename Component>
    void Remove(Entity entity) {
        if (!Has<Component>(entity))
            return;

        auto type_id = std::type_index(typeid(Component));
        auto *component_storage = dynamic_cast<ComponentStorage<Component> *>(m_component_storage[type_id].get());
        component_storage->Remove(entity);
    }

    template<typename Component>
    bool Has(Entity entity) const {
        if (!IsAlive(entity))
            return false;

        auto type_id = std::type_index(typeid(Component));
        if (!m_component_storage.contains(type_id))
            return false;

        auto *component_storage = static_cast<ComponentStorage<Component> *>(m_component_storage.at(type_id).get());
        return component_storage->Has(entity);
    }

    template<typename Component>
    Component &Get(Entity entity) {
        Assert(Has<Component>(entity), "Component not present!");

        auto type_id = std::type_index(typeid(Component));
        auto *component_storage = static_cast<ComponentStorage<Component> *>(m_component_storage[type_id].get());
        return component_storage->Get(entity);
    }

    template<typename Component>
    const Component &Get(Entity entity) const {
        Assert(Has<Component>(entity), "Component not present!");

        auto type_id = std::type_index(typeid(Component));
        auto *component_storage = static_cast<ComponentStorage<Component> *>(m_component_storage.at(type_id).get());
        return component_storage->Get(entity);
    }

    template<typename Component>
    Component *TryGet(Entity entity) {
        return Has<Component>(entity) ? &Get<Component>(entity) : nullptr;
    }

    template<typename Component>
    const Component *TryGet(Entity entity) const {
        return Has<Component>(entity) ? &Get<Component>(entity) : nullptr;
    }

    template<typename Component>
    void Each(const std::function<void(Entity entity, Component &component)> &callback) {
        auto it = m_component_storage.find(std::type_index(typeid(Component)));
        if (it == m_component_storage.end())
            return;

        auto *storage = static_cast<ComponentStorage<Component> *>(it->second.get());

        for (size_t i = 0; i < storage->components.size(); ++i) {
            const Entity entity = storage->entities[i];
            if (IsAlive(entity))
                callback(storage->entities[i], storage->components[i]);
        }
    }
    
    template<typename Component>
    void Each(const std::function<void(Entity entity, const Component &component)> &callback) const {
        auto it = m_component_storage.find(std::type_index(typeid(Component)));
        if (it == m_component_storage.end())
            return;

        auto *storage = static_cast<ComponentStorage<Component> *>(it->second.get());

        for (size_t i = 0; i < storage->components.size(); ++i) {
            const Entity entity = storage->entities[i];
            if (IsAlive(entity))
                callback(storage->entities[i], storage->components[i]);
        }
    }

    Entity FindFirstByName(std::string_view name) const;

    glm::mat4 LocalMatrix(Entity entity);
    glm::mat4 GlobalMatrix(Entity entity);

    void Update();
private:
    struct EntityNode {
        std::string name;
        uint32_t generation = 0;

        bool alive = false;

        Transform cached_transform {};
        bool has_cached_transform = false;

        Entity parent = Entity::Invalid();
        std::vector<Entity> children;

        bool local_dirty = true;
        glm::mat4 local_matrix { 1.0f };
        bool global_dirty = true;
        glm::mat4 global_matrix { 1.0f };
    };

    struct IComponentStorage {
        virtual ~IComponentStorage() = default;
        virtual void Remove(Entity entity) = 0;
    };

    template<typename Component>
    struct ComponentStorage : public IComponentStorage {
        std::vector<uint32_t> indices; // entity.index -> index into entities/components
        std::vector<Entity> entities;
        std::vector<Component> components;

        static const uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();
                
        bool Has(Entity entity) const {
            if (entity.m_index >= indices.size() || !entity.IsValid())
                return false;

            uint32_t index = indices[entity.m_index];
            return index != InvalidIndex
                && index < components.size()
                && entity == entities[index];
        }

        template<typename... Args>
        Component &Add(Entity entity, Args &&...args) {
            if (Has(entity))
                return Get(entity);

            if (indices.size() <= entity.m_index) {
                indices.resize(entity.m_index + 1, InvalidIndex);
            }

            const auto index = static_cast<uint32_t>(components.size());
            indices[entity.m_index] = index;

            entities.push_back(entity);
            components.emplace_back(std::forward<Args>(args)...);

            return components.back();
        }

        void Remove(Entity entity) override {
            if (!Has(entity))
                return;

            if (entity.m_index >= indices.size())
                return;

            uint32_t removed_index = indices[entity.m_index];
            auto last_index = static_cast<uint32_t>(components.size() - 1);

            if (removed_index != last_index) {
                std::swap(components[removed_index], components[last_index]);
                std::swap(entities[removed_index], entities[last_index]);

                indices[entities[removed_index].m_index] = removed_index;
            }

            components.pop_back();
            entities.pop_back();
            indices[entity.m_index] = InvalidIndex;
        }

        Component &Get(Entity entity) {
            Assert(Has(entity), "Component not present!");
            return components[indices[entity.m_index]];
        }

        const Component &Get(Entity entity) const {
            Assert(Has(entity), "Component not present!");
            return components[indices[entity.m_index]];
        }

        Component *TryGet(Entity entity) {
            return Has(entity) ? &Get(entity) : nullptr;
        }

        const Component *TryGet(Entity entity) const {
            return Has(entity) ? &Get(entity) : nullptr;
        }
    };
private:
    AssetManager &m_asset_manager;

    std::vector<EntityNode> m_entities;
    std::vector<uint32_t> m_available_slots;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_component_storage;
private:
    void MarkLocalDirty(Entity entity);
    void MarkGlobalDirty(Entity entity);
    void DetectTransformChange(Entity entity);
    glm::mat4 ComputeLocalTransform(Entity entity);
    glm::mat4 ComputeGlobalTransform(Entity entity);
    void Update(Entity entity);
};

#include "World.h"
#include "Core/Core.h"
#include "Core/errors.h"
#include "Entity.h"
#include "DefaultComponents.h"
#include <string_view>
#include "AssetManager/AssetManager.h"
#include "AssetManager/GLTFModel.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

// =============== //
// ---- World ---- //
// =============== //

World::World(AssetManager &manager, uint32_t initial_entity_capacity)
    : m_asset_manager(manager)
{
    m_entities.reserve(initial_entity_capacity);
}

Entity World::BuildGLTF(GLTFHandle handle) {
    const GLTFModel &gltf = m_asset_manager.GetGLTF(handle);
    Entity root = CreateEntity("GLTF Prefab");

    std::vector<Entity> node_entities(gltf.NodeCount(), Entity::Invalid());

    gltf.ForEachNode([&](const GLTFModel::Node &node) {
        auto [translation, rotation, scale] = node.CalculateTRS();

        std::string name = node.name.empty()
            ? "GLTF Node " + std::to_string(node.index)
            : node.name;

        Entity entity = CreateEntity(name, Transform {
            .translation = translation,
            .rotation = rotation,
            .scale = scale,
        });

        node_entities[node.index] = entity;

        if (node.mesh) {
            Add<GLTFMeshComponent>(entity, GLTFMeshComponent {
                .gltf = handle,
                .node_index = node.index,
                .visible = true,
            });
        }
    });

    // Reproduce the GLTF hierarchy using local transforms.
    gltf.ForEachNode([&](const GLTFModel::Node &node) {
        const Entity entity = node_entities[node.index];
        const Entity parent = node.parent ? node_entities[node.parent->index] : root;

        AttachParent(entity, parent, TransformMethod::Local);
    });

    return root;
}

Entity World::CreateEntity(std::string_view name, const Transform &transform) {
    uint32_t generation = 0;
    uint32_t index = 0;
    if (!m_available_slots.empty()) {
        uint32_t slot = m_available_slots.back();
        m_available_slots.pop_back();

        ENGINE_ASSERT(slot < m_entities.size(), "Entities out of bounds!");
        ENGINE_ASSERT(!m_entities[slot].alive, "Cannot take spot of living entity!");

        index = slot;
        generation = m_entities[index].generation + 1;

        m_entities[index] = EntityNode {
            .name = std::string(name),
            .generation = generation,
            .alive = true,
        };
    } else {
        generation = 0;

        index = static_cast<uint32_t>(m_entities.size());
        m_entities.push_back(EntityNode{
            .name = std::string(name),
            .generation = generation,
            .alive = true,
        });
    }

    Entity entity;
    entity.m_index = index;
    entity.m_generation = generation;

    Add<Transform>(entity, transform);    

    return entity;
}

void World::DestroyEntity(Entity entity, TransformMethod transform_method) {
    if (!entity.IsValid() || !IsAlive(entity))
        return;

    ENGINE_ASSERT(entity.m_index < m_entities.size(), "Entity out of bounds!");
    EntityNode &data = m_entities[entity.m_index];

    while (!data.children.empty())
        DetachParent(data.children.back(), transform_method);

    DetachParent(entity, TransformMethod::Local);

    for (auto &[type, storage] : m_component_storage) {
        storage->Remove(entity);
    }

    data.name = "";
    data.alive = false;
    m_available_slots.push_back(entity.m_index);
};

void World::DestroyEntityTree(Entity entity) {
    if (!entity.IsValid() || !IsAlive(entity))
        return;

    ENGINE_ASSERT(entity.m_index < m_entities.size(), "Entity out of bounds!");
    EntityNode &data = m_entities[entity.m_index];

    while (!data.children.empty())
        DestroyEntityTree(data.children.back());

    DestroyEntity(entity, TransformMethod::Local);
}

bool World::IsValid(Entity entity) const {
    return entity.IsValid()
        && entity.m_index < m_entities.size()
        && entity.m_generation == m_entities[entity.m_index].generation;
}

bool World::IsAlive(Entity entity) const {
    return IsValid(entity) && m_entities[entity.m_index].alive;
}

std::string_view World::Name(Entity entity) const {
    if (!IsAlive(entity))
        return "";

    return m_entities[entity.m_index].name;
}

void World::SetName(Entity entity, std::string_view name) {
    if (!IsAlive(entity))
        return;

    m_entities[entity.m_index].name = name;  
}

void World::AttachParent(Entity child, Entity parent, TransformMethod method) {
    if (!IsAlive(child) || !IsAlive(parent) || child == parent)
        return;

    // Prevent cycles.
    for (Entity current = parent; IsAlive(current); current = Parent(current)) {
        if (current == child)
            return;
    }

    glm::mat4 old_global { 1.0f };

    if (method == TransformMethod::Global)
        old_global = GlobalMatrix(child);

    DetachParent(child, TransformMethod::Local);

    m_entities[child.m_index].parent = parent;
    m_entities[parent.m_index].children.push_back(child);

    if (method == TransformMethod::Global) {
        auto &transform = Get<Transform>(child);

        transform = Transform::FromMatrix(
            glm::inverse(GlobalMatrix(parent)) * old_global
        );

        MarkLocalDirty(child);
    } else {
        MarkGlobalDirty(child);
    }
}

void World::DetachParent(Entity child, TransformMethod method) {
    if (!IsAlive(child))
        return;

    glm::mat4 old_global { 1.0f };

    if (method == TransformMethod::Global)
        old_global = GlobalMatrix(child);

    EntityNode &child_node = m_entities[child.m_index];
    const Entity parent = child_node.parent;

    if (IsAlive(parent)) {
        auto &children = m_entities[parent.m_index].children;

        const auto it = std::ranges::find(children, child);
        if (it != children.end())
            children.erase(it);
    }

    child_node.parent = Entity::Invalid();

    if (method == TransformMethod::Global) {
        Get<Transform>(child) = Transform::FromMatrix(old_global);
        MarkLocalDirty(child);
    } else {
        MarkGlobalDirty(child);
    }
}

Entity World::Parent(Entity entity) const {
    if (!IsAlive(entity))
        return Entity::Invalid();

    return m_entities[entity.m_index].parent;
}

std::span<const Entity> World::Children(Entity entity) const {
    if (!IsAlive(entity))
        return {};

    return m_entities[entity.m_index].children;
}

Entity World::Root(Entity entity) const {
    Entity current = entity;
    while (Parent(current) != Entity::Invalid()) {
        current = Parent(current);
    }
    return current;
};

Entity World::FindFirstByName(std::string_view name) const {
    for (uint32_t i = 0; i < m_entities.size(); ++i) {
        const EntityNode &node = m_entities[i];

        if (!node.alive || node.name != name)
            continue;

        Entity entity;
        entity.m_index = i;
        entity.m_generation = node.generation;
        return entity;
    }

    return Entity::Invalid();
}

glm::mat4 World::LocalMatrix(Entity entity) {
    return ComputeLocalTransform(entity);
}

glm::mat4 World::GlobalMatrix(Entity entity) {
    ENGINE_PROFILER_FUNCTION();

    return ComputeGlobalTransform(entity);
}

std::tuple<glm::vec3, glm::quat, glm::vec3> World::WorldTRS(Entity entity) {
    ComputeGlobalTransform(entity);

    EntityNode &node = m_entities[entity.m_index];
    return std::make_tuple(node.world_position, node.world_rotation, node.world_scale);
}

glm::vec3 World::WorldPosition(Entity entity) {
    return std::get<0>(WorldTRS(entity));
}

glm::quat World::WorldRotation(Entity entity) {
    return std::get<1>(WorldTRS(entity));
}

glm::vec3 World::WorldScale(Entity entity) {
    return std::get<2>(WorldTRS(entity));
}

void World::Update() {
    for (uint32_t i = 0; i < m_entities.size(); ++i) {
        EntityNode& node = m_entities[i];

        if (!node.alive || IsAlive(node.parent))
            continue;

        Entity entity;
        entity.m_index = i;
        entity.m_generation = node.generation;

        Update(entity);
    }
}

void World::MarkGlobalDirty(Entity entity) {
    if (!IsAlive(entity))
        return;

    EntityNode &node = m_entities[entity.m_index];

    node.global_dirty = true;

    for (auto &child : node.children) {
        MarkGlobalDirty(child);
    }
}

void World::MarkLocalDirty(Entity entity) {
    if (!IsAlive(entity))
        return;

    m_entities[entity.m_index].local_dirty = true;
    MarkGlobalDirty(entity);
}

glm::mat4 World::ComputeLocalTransform(Entity entity) {
    ENGINE_ASSERT(IsAlive(entity), "Cannot compute local transformation of non-alive entity");
    
    DetectTransformChange(entity);
    EntityNode &node = m_entities[entity.m_index];

    if (node.local_dirty) {
        auto *transform = TryGet<Transform>(entity);
        node.local_matrix = transform
            ? transform->CalculateLocalMatrix()
            : glm::mat4(1.0f);

        node.local_dirty = false;
    }
    return node.local_matrix;
}


glm::mat4 World::ComputeGlobalTransform(Entity entity) {
    ENGINE_ASSERT(IsAlive(entity), "Cannot compute local transformation of non-alive entity");

    DetectTransformChange(entity);

    EntityNode &node = m_entities[entity.m_index];
    const Entity parent = node.parent;

    if (IsAlive(parent)) {
        ComputeGlobalTransform(parent);
    }

    if (node.global_dirty) {
        const glm::mat4 local = ComputeLocalTransform(entity);

        node.global_matrix = IsAlive(parent)
            ? m_entities[parent.m_index].global_matrix * local
            : local;

        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(node.global_matrix, node.world_scale, node.world_rotation, node.world_position, skew, perspective);

        node.global_dirty = false;
    }

    return node.global_matrix;
}

void World::DetectTransformChange(Entity entity) {
    EntityNode &node = m_entities[entity.m_index];
    const Transform *transform = TryGet<Transform>(entity);

    if (transform == nullptr) {
        if (node.has_cached_transform) {
            node.has_cached_transform = false;
            MarkLocalDirty(entity);
        }

        return;
    }

    if (!node.has_cached_transform || *transform != node.cached_transform) {
        node.cached_transform = *transform;
        node.has_cached_transform = true;

        MarkLocalDirty(entity);
    }
}

void World::Update(Entity entity) {
    if (!IsAlive(entity))
        return;

    ComputeGlobalTransform(entity);

    for (auto &child : Children(entity)) {
        Update(child);
    }
}

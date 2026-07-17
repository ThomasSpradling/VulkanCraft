#pragma once

#include <cstdint>
#include <limits>

class World;
class Entity {
    friend World;
public:
    Entity() = default;

    static constexpr Entity Invalid() {
        Entity entity;
        entity.m_generation = std::numeric_limits<uint32_t>::max();
        entity.m_index = std::numeric_limits<uint32_t>::max();
        return entity;
    }

    bool constexpr IsValid() const {
        Entity invalid = Entity::Invalid();
        return invalid.m_index != m_index && invalid.m_generation != m_generation;
    }

    explicit constexpr operator bool() const { return IsValid(); }

    friend bool operator==(Entity, Entity) = default;
private:
    uint32_t m_index = std::numeric_limits<uint32_t>::max();
    uint32_t m_generation = std::numeric_limits<uint32_t>::max();
};

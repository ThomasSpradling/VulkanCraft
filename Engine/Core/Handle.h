#pragma once

#include <cstdint>
#include <limits>

template <typename Tag>
class Handle {
public:
    Handle() = default;
    Handle(uint32_t index, uint32_t generation)
        : m_index(index)
        , m_generation(generation) 
    {}
    Handle(uint64_t value)
        : m_index(static_cast<uint32_t>(value >> 32))
        , m_generation(static_cast<uint32_t>(value))
    {}

    static constexpr Handle Invalid() {
        Handle handle;
        handle.m_generation = std::numeric_limits<uint32_t>::max();
        handle.m_index = std::numeric_limits<uint32_t>::max();
        return handle;
    }

    bool constexpr IsValid() const {
        Handle invalid = Handle::Invalid();
        return invalid.m_index != m_index && invalid.m_generation != m_generation;
    }

    uint64_t constexpr ToInt() const {
        return (static_cast<uint64_t>(m_index) << 32) | static_cast<uint64_t>(m_generation);
    };

    explicit constexpr operator uint64_t() const {
        return ToInt();
    }

    explicit constexpr operator bool() const { return IsValid(); }
    friend constexpr bool operator==(Handle, Handle) = default;

    uint32_t Index() { return m_index; }
    uint32_t Generation() { return m_generation; }
private:
    uint32_t m_index = std::numeric_limits<uint32_t>::max();
    uint32_t m_generation = std::numeric_limits<uint32_t>::max();
};

//// Default Handles ////

struct AnimationHandleTag;
struct BufferHandleTag;
struct GLTFHandleTag;
struct MaterialHandleTag;
struct MeshHandleTag;
struct TextureHandleTag;
struct SamplerHandleTag;
struct ShaderHandleTag;

using AnimationHandle = Handle<AnimationHandleTag>;
using BufferHandle = Handle<BufferHandleTag>;
using GLTFHandle = Handle<GLTFHandleTag>;
using MaterialHandle = Handle<MaterialHandleTag>;
using MeshHandle = Handle<MeshHandleTag>;
using TextureHandle = Handle<TextureHandleTag>;
using SamplerHandle = Handle<SamplerHandleTag>;
using ShaderHandle = Handle<ShaderHandleTag>;

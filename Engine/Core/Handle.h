#pragma once

#include <cstdint>
#include <limits>

class AssetManager;
template <typename Tag>
class Handle {
    friend AssetManager;
public:
    Handle() = default;

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

    explicit constexpr operator bool() const { return IsValid(); }

    friend constexpr bool operator==(Handle, Handle) = default;
private:
    uint32_t m_index = std::numeric_limits<uint32_t>::max();
    uint32_t m_generation = std::numeric_limits<uint32_t>::max();
};

//// Default Handles ////

struct MeshHandleTag;
struct MaterialHandleTag;
struct TextureHandleTag;
struct SamplerHandleTag;
struct ShaderHandleTag;
struct GLTFHandleTag;
struct BufferHandleTag;

using MeshHandle = Handle<MeshHandleTag>;
using MaterialHandle = Handle<MaterialHandleTag>;
using TextureHandle = Handle<TextureHandleTag>;
using SamplerHandle = Handle<SamplerHandleTag>;
using ShaderHandle = Handle<ShaderHandleTag>;
using GLTFHandle = Handle<GLTFHandleTag>;
using BufferHandle = Handle<BufferHandleTag>;

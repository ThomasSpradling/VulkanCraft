#pragma once

#include "Common.h"
#include <string>

enum class BufferUsageBits : uint8_t {
    None = 0,
    Index       = 1 << 0,
    Vertex      = 1 << 1,
    Uniform     = 1 << 2,
    Storage     = 1 << 3,
    Indirect    = 1 << 4,
};

inline BufferUsageBits operator|(BufferUsageBits a, BufferUsageBits b) {
    return static_cast<BufferUsageBits>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline BufferUsageBits operator&(BufferUsageBits a, BufferUsageBits b) {
    return static_cast<BufferUsageBits>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

struct GPUBufferData {
    BufferUsageBits usage = BufferUsageBits::None;
    StorageType storage_type = StorageType::HostVisible;
    size_t size = 0;
    const void *data = nullptr;
    std::string debug_name {};
};

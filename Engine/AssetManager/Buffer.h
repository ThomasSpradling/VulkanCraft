#pragma once

#include <string>

enum class BufferUsageBits : uint8_t {
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

enum class StorageType : uint8_t {
    Device = 0,     // Stored and only visible on GPU device
    HostVisible,    // Stored on GPU device, but is also mapped to host
    MemoryLess,
};

struct GPUBufferData {
    BufferUsageBits usage;
    StorageType storage_type = StorageType::HostVisible;
    size_t size = 0;
    const void *data = nullptr;
    std::string debug_name {};
};

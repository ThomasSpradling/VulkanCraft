#pragma once

#include <cstdint>

enum class StorageType : uint8_t {
    Device = 0,     // Stored and only visible on GPU device
    HostVisible,    // Stored on GPU device, but is also mapped to host
    MemoryLess,
};

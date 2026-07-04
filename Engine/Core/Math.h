#pragma once

#include <cmath>
#include <cstdint>

// The largest number of bytes that can be streamed in `milliseconds` at rate kbps (kilobits per second)
inline uint32_t KbpsToBytes(uint32_t kbps, double milliseconds) {
    return static_cast<uint32_t>(
        std::floor(static_cast<double>(kbps) * static_cast<double>(milliseconds)) / 8.0
    );
}
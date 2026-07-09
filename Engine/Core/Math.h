#pragma once

#include <cmath>
#include <cstdint>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/glm.hpp>
#include <limits>
#include <type_traits>

template <typename T>
consteval T MakeEpsilon() {
    if constexpr (std::is_floating_point_v<T>)
        return std::numeric_limits<T>::epsilon();
    else
        return T{0};
}

template <typename T>
inline constexpr T Epsilon = MakeEpsilon<T>();

template <typename T>
inline constexpr T Infinity = std::numeric_limits<T>::max();

// The largest number of bytes that can be streamed in `milliseconds` at rate kbps (kilobits per second)
inline uint32_t KbpsToBytes(uint32_t kbps, double milliseconds) {
    return static_cast<uint32_t>(
        std::floor(static_cast<double>(kbps) * static_cast<double>(milliseconds)) / 8.0
    );
}

template<glm::length_t L, typename T, glm::qualifier Q>
inline bool NearlyEqual(const glm::vec<L, T, Q> &vec1, const glm::vec<L, T, Q> &vec2, T threshold = Epsilon<T>) {
    for (uint32_t i = 0; i < L; ++i) {
        if (vec1[i] - vec2[i] > threshold || vec2[i] - vec1[i] > threshold)
            return false;
    }
    return true;
}
#pragma once

#include <glm/detail/qualifier.hpp>
#include <glm/glm.hpp>
#include <ostream>

template <glm::length_t L, typename T, glm::qualifier Q>
std::ostream &operator<<(std::ostream &stream, const glm::vec<L, T, Q> &vec) {
    if (typeid(T) == typeid(uint32_t)) {
        stream << "UVec";
    } else if (typeid(T) == typeid(int32_t)) {
        stream << "IVec";
    } else if (typeid(T) == typeid(float)) {
        stream << "Vec";
    }

    stream << L << "(";
    for (glm::length_t i = 0; i < L; ++i) {
        stream << vec[i];
        if (i < L - 1)
            stream << ", ";
    }
    stream << ")";
    return stream;
}

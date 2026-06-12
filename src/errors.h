#pragma once
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>

template<typename T>
inline void Assert(T &&assertion, const std::string &message, const char *file = "", const int line = 0) {
    if (!assertion) {
        std::ostringstream oss;
        oss << "Assertion failed";
        if (line > 0)
            oss << " at Line " << line;
        if (std::strlen(file) > 0)
            oss << " in file " << file;
        oss << ". ";
        oss << message;
        throw std::runtime_error(oss.str());
    }
}
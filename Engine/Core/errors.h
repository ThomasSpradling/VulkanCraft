#pragma once
#include <cstring>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#if ENGINE_ENABLE_ASSERTIONS
inline void HandleAssertion(std::string_view expression, std::string_view message, const std::source_location location = std::source_location::current()) {
    std::ostringstream oss;
    oss << "Assertion failed! (" << expression << ")\n"
        << "\tat line " << location.line() << " in " << location.file_name() <<".\n"
        << "\t" << message;

    throw std::runtime_error(oss.str());
}

#define ENGINE_ASSERT(expression, message)                                    \
    do {                                                                      \
        if (!(expression)) [[unlikely]] {                                     \
            HandleAssertion(#expression, message, std::source_location::current()); \
        }                                                                     \
    } while(false);

#else
#define ENGINE_ASSERT(condition, message)                             \
    do {                                                              \
        static_cast<void>(sizeof(static_cast<bool>(condition)));      \
        static_cast<void>(sizeof((message)));                         \
    } while (false)
#endif

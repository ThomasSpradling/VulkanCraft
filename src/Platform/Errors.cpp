#include "Errors.h"

std::string WindowsErrorString(DWORD error_code) {
    char* message = nullptr;

    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<char*>(&message),
        0,
        nullptr
    );

    if (size == 0) {
        return "Unknown Windows error code: " + std::to_string(error_code);
    }

    std::string result(message, size);
    LocalFree(message);

    while (!result.empty() &&
           (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }

    return result;
}

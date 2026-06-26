#include "SocketAPI.h"
#include <stdexcept>

void SocketAPI::Initialize() {
#ifdef PLATFORM_WINDOWS
    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != NO_ERROR)
        throw std::runtime_error("Failed to start WinSock!");
#endif // PLATFORM_WINDOWS
    m_ok = true;
}

SocketAPI::~SocketAPI() {
#ifdef PLATFORM_WINDOWS
    if (m_ok)
        WSACleanup();
#endif // PLATFORM_WINDOWS
}

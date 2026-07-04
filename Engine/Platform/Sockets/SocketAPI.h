#pragma once
#include "Platform/Platform.h"
#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"

#ifdef PLATFORM_WINDOWS
    #define NOMINMAX
    #include <WinSock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
#endif

class SocketAPI : public NonCopyable, public NonMovable {
public:
    SocketAPI() = default;

    // Call this before working with any socket information!
    void Initialize();
    ~SocketAPI();
private:
    bool m_ok = false;
};


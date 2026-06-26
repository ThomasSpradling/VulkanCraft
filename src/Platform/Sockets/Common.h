#pragma once

#include "../Platform.h"

#ifdef PLATFORM_WINDOWS
    #define NOMINMAX
    #include <WinSock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <fcntl.h>
#endif

#include "../Errors.h"
#include "../../errors.h"

#ifdef PLATFORM_WINDOWS
using SocketHandle = SOCKET;
#else
using SocketHandle = int;
#endif

enum class AddressFamily : int {
    None = AF_UNSPEC,
    IPv4 = AF_INET,
    IPv6 = AF_INET6,
};

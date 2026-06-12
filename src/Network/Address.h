#pragma once

#include <cstring>
#include <string>
#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <cstdint>

struct NetworkAddress {
    uint32_t address = 0;
    uint16_t port = 0;

    bool operator==(const NetworkAddress &other) const {
        return address == other.address && port == other.port;
    }

    bool operator!=(const NetworkAddress &other) const {
        return !(*this == other);
    }
};

inline sockaddr ToSockAddr(const NetworkAddress &address) {
    static_assert(sizeof(sockaddr) >= sizeof(sockaddr_in));

    sockaddr_in socket_address {};
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = htonl(address.address);
    socket_address.sin_port = htons(address.port);

    sockaddr result {};
    std::memcpy(&result, &socket_address, sizeof(socket_address));

    return result;
}

inline NetworkAddress FromSockAddr(const sockaddr &socket_address) {
    sockaddr_in addr {};
    std::memcpy(&addr, &socket_address, sizeof(addr));

    return NetworkAddress {
        .address = ntohl(addr.sin_addr.s_addr),
        .port = ntohs(addr.sin_port),
    };
}

inline std::string GetHostName(int ai_family, const sockaddr &address) {
    const sockaddr_in *address_in = reinterpret_cast<const sockaddr_in *>(&address);
    const void *addr = &(address_in->sin_addr);

    std::string ip_address;
    ip_address.resize(INET6_ADDRSTRLEN);
    inet_ntop(ai_family, addr, ip_address.data(), ip_address.size());
    return ip_address;
}

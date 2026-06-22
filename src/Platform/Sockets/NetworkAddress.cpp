#include "NetworkAddress.h"

NetworkAddress::NetworkAddress(const sockaddr *address, socklen_t size) {
    Assert(address != nullptr, "Cannot create socket address!");
    Assert(sizeof(m_address) >= size, "Cannot handle address of size " + std::to_string(size));
    std::memcpy(&m_address, address, size);

    m_size = size;
    m_family = static_cast<AddressFamily>(address->sa_family);

    m_name = GetNameFromAddress();
    m_service = GetService();
}


NetworkAddress::NetworkAddress(uint32_t port, AddressFamily family) {
    addrinfo *address_info;
    addrinfo hints = {
        .ai_flags = AI_PASSIVE,
        .ai_family = static_cast<int>(family),
        .ai_socktype = SOCK_DGRAM,
    };

    if (int error = getaddrinfo(nullptr, std::to_string(port).data(), &hints, &address_info); error != 0) {
        throw std::runtime_error("Error getting address info: " + std::string(gai_strerrorA(error)));
    }

    std::memcpy(&m_address, address_info->ai_addr, address_info->ai_addrlen);
    m_size = static_cast<socklen_t>(address_info->ai_addrlen);
    m_family = static_cast<AddressFamily>(address_info->ai_family);
    freeaddrinfo(address_info);

    m_name = GetNameFromAddress();
    m_service = GetService();
};

NetworkAddress::NetworkAddress(std::string_view address, uint32_t port,  AddressFamily family) {
    std::string host(address);
    
    addrinfo *address_info;
    addrinfo hints = {
        .ai_family = static_cast<int>(family),
        .ai_socktype = SOCK_DGRAM,
    };

    if (int error = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &address_info); error != 0) {
        throw std::runtime_error("Error getting address info: " + std::string(gai_strerrorA(error)));
    }

    std::memcpy(&m_address, address_info->ai_addr, address_info->ai_addrlen);
    m_size = static_cast<socklen_t>(address_info->ai_addrlen);
    m_family = static_cast<AddressFamily>(address_info->ai_family);
    freeaddrinfo(address_info);
    
    m_name = GetNameFromAddress();
    m_service = GetService();
}

bool NetworkAddress::operator==(const NetworkAddress &other) const {
    if (m_family != other.m_family)
        return false;
    if (m_size != other.m_size)
        return false;
    return m_name == other.m_name && m_service == other.m_service;
}

std::string NetworkAddress::GetNameFromAddress() const {
    char host[NI_MAXHOST]{};

    int error = getnameinfo(
        Data(),
        m_size,
        host,
        sizeof(host),
        nullptr,
        0,
        NI_NUMERICHOST
    );

    if (error != 0) {
        throw std::runtime_error("getnameinfo failed: " + std::string(gai_strerrorA(error)));
    }

    return std::string(host);
}

uint16_t NetworkAddress::GetService() const {
    if (m_family == AddressFamily::IPv4) {
        const auto *addr = reinterpret_cast<const sockaddr_in *>(&m_address);
        return ntohs(addr->sin_port);
    }

    if (m_family == AddressFamily::IPv6) {
        const auto *addr = reinterpret_cast<const sockaddr_in6 *>(&m_address);
        return ntohs(addr->sin6_port);
    }

    throw std::runtime_error("Cannot get service from unknown address family.");
}

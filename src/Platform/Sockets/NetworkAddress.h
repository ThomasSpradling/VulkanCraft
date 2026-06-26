#pragma once

#include <string>
#include <string_view>
#include "Common.h"
#include "../../Utils/NonMovable.h"

class Socket;
class NetworkAddress : public NonMovable {
    friend Socket;
public:
    NetworkAddress() {}
    explicit NetworkAddress(const sockaddr *address, socklen_t size);
    explicit NetworkAddress(uint32_t port, AddressFamily family = AddressFamily::IPv4);
    explicit NetworkAddress(std::string_view address, uint32_t port, AddressFamily family = AddressFamily::IPv4);

    ~NetworkAddress() = default;
    NetworkAddress(const NetworkAddress &other) = default;
    NetworkAddress &operator=(const NetworkAddress &other) = default;
    
    bool operator==(const NetworkAddress &other) const;

    static NetworkAddress Any(uint32_t port, AddressFamily family = AddressFamily::IPv4) {
        return NetworkAddress{port, family};
    }

    static NetworkAddress Localhost(uint32_t port, AddressFamily family = AddressFamily::IPv4) {
        switch (family) {
            case AddressFamily::IPv6:
                return NetworkAddress{"::1", port, family};
            default:
            case AddressFamily::IPv4:
                return NetworkAddress{"127.0.0.1", port, family};
        }
    }

    const sockaddr *Data() const { return reinterpret_cast<const sockaddr *>(&m_address); }
    sockaddr *Data() { return reinterpret_cast<sockaddr *>(&m_address); }

    std::string AddressName() const { return m_name; }
    uint16_t Port() const { return m_service; }

    AddressFamily GetAddressFamily() const { return m_family; }
private:
    std::string m_name {};
    sockaddr_storage m_address {};
    uint16_t m_service {};

    socklen_t m_size {};
    AddressFamily m_family {};
private:
    std::string GetNameFromAddress() const;
    uint16_t GetService() const;
};
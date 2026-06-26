#pragma once

#include "NetworkBuffer.h"

#include "../../Utils/NonCopyable.h"
#include "../../Utils/NonMovable.h"
#include "Common.h"
#include "NetworkAddress.h"

constexpr uint32_t MAX_NETWORK_TRANSMISSION_SIZE = 1200;

enum class SocketError : uint8_t {
    None = 0,
    WouldBlock,
};

class Socket : public NonCopyable, public NonMovable {
public:
    Socket(AddressFamily address_family);
    ~Socket();
    void Bind(const NetworkAddress &socket_address);
    bool IsValid() const;
    void Close();

    // Send the first `MAX_SOCKET_PACKET_SIZE` bytes to the target address
    uint32_t SendTo(const NetworkAddress &to, const NetworkBuffer &data);

    // Receive up to `MAX_SOCKET_PACKET_SIZE` bytes from a source
    SocketError ReceiveFrom(NetworkAddress &from, NetworkBuffer &data);

    void MakeNonBlocking(bool value = true);
private:
    void SocketError(const std::string &description);

    int m_family {};
    SocketHandle m_handle = 0;
};

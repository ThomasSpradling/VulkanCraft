#pragma once

#include "ISocket.h"
#include "NetworkBuffer.h"

#include "Common.h"
#include "NetworkAddress.h"

class Socket : public ISocket {
public:
    Socket(AddressFamily address_family);
    ~Socket() override;

    void Bind(const NetworkAddress &socket_address) override;
    bool IsValid() const override;
    void Close() override;

    uint32_t SendTo(const NetworkAddress &to, const NetworkBuffer &data) override;
    SocketError ReceiveFrom(NetworkAddress &from, NetworkBuffer &data) override;

    void MakeNonBlocking(bool value = true) override;
private:
    void SocketError(const std::string &description);

    int m_family {};
    SocketHandle m_handle = 0;
};

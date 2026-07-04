#pragma once
#include "Core/NonCopyable.h"
#include "Core/NonMovable.h"
#include "NetworkAddress.h"
#include "NetworkBuffer.h"

constexpr uint32_t MAX_NETWORK_TRANSMISSION_SIZE = 1200;

enum class SocketError : uint8_t {
    None = 0,
    WouldBlock,
};

class ISocket : public NonCopyable, public NonMovable {
public:
    ISocket(AddressFamily address_family) {};
    virtual ~ISocket() = default;

    virtual void Bind(const NetworkAddress &socket_address) = 0;
    virtual bool IsValid() const { return true; }
    virtual void Close() = 0;

    virtual uint32_t SendTo(const NetworkAddress &to, const NetworkBuffer &data) = 0;
    virtual SocketError ReceiveFrom(NetworkAddress &from, NetworkBuffer &data) = 0;

    virtual void MakeNonBlocking(bool value = true) {};
};

#include "Socket.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>
#include "../../errors.h"

Socket::Socket(AddressFamily address_family) {
    m_family = static_cast<int>(address_family);
    m_handle = socket(m_family, SOCK_DGRAM, IPPROTO_UDP);
}

Socket::~Socket() {
    Close();
}

void Socket::Bind(const NetworkAddress &socket_address) {
    if (bind(m_handle, socket_address.Data(), socket_address.m_size) == SOCKET_ERROR) {
        throw std::runtime_error("Error binding socket: " + WindowsErrorString(WSAGetLastError()));
    }
}

bool Socket::IsValid() const {
#ifdef PLATFORM_WINDOWS
    return m_handle != INVALID_SOCKET;
#else
    return m_handle >= 0;
#endif
}

void Socket::Close() {
#ifdef PLATFORM_WINDOWS
    if (m_handle != INVALID_SOCKET) {
        closesocket(m_handle);
        m_handle = INVALID_SOCKET;
    }
#else
    if (m_handle >= 0) {
        close(m_handle);
        m_handle = -1;
    }
#endif
}

uint32_t Socket::SendTo(const NetworkBuffer &data, const NetworkAddress &to) {
    uint32_t data_size = std::min<uint32_t>(data.GetSize(), MAX_NETWORK_TRANSMISSION);
    if (data_size == 0)
        return 0;

    int data_sent = sendto(m_handle, data.GetData(), data_size, 0, to.Data(), to.m_size);
    if (data_sent == SOCKET_ERROR) {
        throw std::runtime_error("Error sending from socket: " + WindowsErrorString(WSAGetLastError()));
    }
    return data_sent;
}

SocketError Socket::ReceiveFrom(NetworkBuffer &buffer, NetworkAddress &from) {
    sockaddr_storage addr {};
    socklen_t size = sizeof(addr);
    
    buffer.Resize(MAX_NETWORK_TRANSMISSION);
    int data_received = recvfrom(m_handle, buffer.GetData(), buffer.GetSize(), 0, reinterpret_cast<sockaddr*>(&addr), &size);
    if (data_received == SOCKET_ERROR) {
#ifdef PLATFORM_WINDOWS
        int error = WSAGetLastError();

        if (error == WSAEWOULDBLOCK)
            return SocketError::WouldBlock;

        if (error == WSAECONNRESET)
            return SocketError::WouldBlock;
#else
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return SocketError::WouldBlock;

        if (errno == ECONNRESET || errno == ECONNREFUSED)
            return SocketError::WouldBlock;
#endif

        throw std::runtime_error("Error receiving to socket: " + WindowsErrorString(WSAGetLastError()));
    }

    NetworkAddress recv_address{reinterpret_cast<const sockaddr*>(&addr), size};
    buffer.Resize(data_received);
    from = recv_address;

    return SocketError::None;
}

void Socket::MakeNonBlocking(bool value) {
    u_long non_blocking = static_cast<u_long>(value);
    if (ioctlsocket(m_handle, FIONBIO, &non_blocking) == SOCKET_ERROR) {
        throw std::runtime_error("Error changing I/O Socket Control Non-Blocking Mode: " + WindowsErrorString(WSAGetLastError()));
    }
}

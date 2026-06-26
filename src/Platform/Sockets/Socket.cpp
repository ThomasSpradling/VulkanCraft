#include "Socket.h"
#include <algorithm>
#include <cstring>
#include <format>
#include <stdexcept>
#include <utility>
#include "../../errors.h"

Socket::Socket(AddressFamily address_family) {
    m_family = static_cast<int>(address_family);
    m_handle = socket(m_family, SOCK_DGRAM, IPPROTO_UDP);

    if (m_handle == INVALID_SOCKET)
        SocketError("Failed to create socket.");
}

Socket::~Socket() {
    Close();
}

void Socket::Bind(const NetworkAddress &socket_address) {
    if (bind(m_handle, socket_address.Data(), socket_address.m_size) == SOCKET_ERROR) {
        SocketError("Error binding socket.");
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

uint32_t Socket::SendTo(const NetworkAddress &to, const NetworkBuffer &data) {
    Assert(data.GetSize() <= MAX_NETWORK_TRANSMISSION_SIZE, "Cannot send more data than MTU!");
    if (data.GetSize() == 0)
        return 0;

    int data_sent = sendto(m_handle, data.GetData(), data.GetSize(), 0, to.Data(), to.m_size);
    if (data_sent == SOCKET_ERROR)
        SocketError("Error sending from socket.");

    return data_sent;
}

SocketError Socket::ReceiveFrom(NetworkAddress &from, NetworkBuffer &buffer) {
    sockaddr_storage addr {};
    socklen_t size = sizeof(addr);
    
    buffer.Resize(MAX_NETWORK_TRANSMISSION_SIZE);
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

        SocketError("Error receiving to socket.");
    }

    NetworkAddress recv_address{reinterpret_cast<const sockaddr*>(&addr), size};
    buffer.Resize(data_received);
    from = recv_address;

    return SocketError::None;
}

void Socket::MakeNonBlocking(bool value) {
#ifdef PLATFORM_WINDOWS
    DWORD non_blocking = static_cast<DWORD>(value);
    if (ioctlsocket(m_handle, FIONBIO, &non_blocking) == SOCKET_ERROR) {
        SocketError("Error changing I/O Socket Control Non-Blocking Mode.");
    }
#else
    int non_blocking = 1;
    if (fcntl(m_handle, F_SETFL, O_NONBLOCK, non_blocking) == -1) {
        SocketError("Error changing I/O Socket Control Non-Blocking Mode.")
    }
#endif
}

void Socket::SocketError(const std::string &description) {
#ifdef PLATFORM_WINDOWS
    int err = WSAGetLastError();
    throw std::runtime_error(std::format("{} Error [{}]: {}.", description, err, WindowsErrorString(err)));
#else
    throw std::runtime_error(description);
#endif
}

#pragma once

#include <memory>
#include <optional>
#include "../Platform/Sockets/Socket.h"
#include "Address.h"
#include "Packets.h"

enum class ConnectState : uint8_t {
    Disconnected = 0,
    Connecting,
    Connected,
};

enum class HostType : uint8_t {
    Client,
    Server,
};

// enum class PacketSendFlagBit : uint8_t {
//     Reliable    = 1 << 0,
//     Ordered     = 1 << 0,
//     Reliable    = 1 << 0,
//     Reliable    = 1 << 0,
// };

struct NetworkPeer {
    NetworkAddress address {};
    ConnectState connected = ConnectState::Disconnected;
};

class NetworkHost : public NonMovable, public NonCopyable {
public:
    NetworkHost(HostType type, uint32_t max_peers = 32);
    NetworkHost(HostType type, const NetworkAddress &network_address, uint32_t max_peers = 32);
    ~NetworkHost() = default;

    std::optional<Packet> ReceivePacket(NetworkPeer &peer);
    void SendPacket(const NetworkPeer &peer, const Packet &packet, bool reliable = false);

    std::optional<NetworkPeer> ConnectToHost(const NetworkAddress &address);
    void DisconnectFromHost(const NetworkAddress &address);
private:
    std::optional<NetworkAddress> m_address = std::nullopt;
    std::unique_ptr<Socket> m_socket;
    
    const uint32_t m_max_peers;
    std::vector<NetworkPeer> m_peers;
    HostType m_type;
private:
    void HandleConnectionPackets();
    void SendRawPacket(const NetworkAddress &address, const Packet &packet);

    std::optional<uint32_t> GetAddressSlot(const NetworkAddress &address);
    std::optional<uint32_t> FindEmptySlot();

    // For unreliable sending of large data
    std::vector<Packet> SplitIntoFragments(const Packet &packet);

    // For reliable sending of large data
    std::vector<Packet> SplitIntoSlices(uint16_t chunk_id, const Packet &packet);

    uint16_t m_current_sequence = 0;
};

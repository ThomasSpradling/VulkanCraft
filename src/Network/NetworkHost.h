#pragma once

#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <vector>
#include "../Platform/Sockets/Socket.h"
#include "Packet.h"

enum class PeerStatus : uint8_t {
    Disconnected = 0,
    Disconnecting,
    Connected,
    Connecting,
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

enum class NetworkCommandType {
    Connect,
    Disconnect,
    ReceivePacket,
};

using PeerId = uint32_t;
using ChannelId = uint8_t;

// A command represents an incomming full packet for use by the application;
// if an incoming packet is not complete (e.g. a fragment or un-acked reliable message),
// we must process it before emitting such a command
struct NetworkCommand {
    NetworkCommandType type = NetworkCommandType::Connect;
    PeerId peer = std::numeric_limits<uint32_t>::max();
    std::unique_ptr<Packet> packet {};
};

struct NetworkChannel : public NonCopyable {
    std::queue<NetworkCommand> incoming_command_queue {}; // recv'd commands from this peer
};

struct NetworkPeer : public NonCopyable {
public:
    NetworkPeer(uint8_t max_channels)
        : channels(max_channels) {}

    struct Timers {
        double connect_request    = 0.0;
        double connect_attempt    = 0.0;
        double disconnect_request = 0.0;
        double disconnect_attempt = 0.0;
    } timers {};
    double max_connect_wait_time    = 1000.0; // ms
    double max_disconnect_wait_time = 1000.0; // ms

    uint32_t max_packet_send_bandwidth = 256; // kbps

    PeerId peer_id = std::numeric_limits<uint32_t>::max();
    NetworkAddress address {};
    PeerStatus status = PeerStatus::Disconnected;

    std::vector<NetworkChannel> channels {};

    std::queue<std::unique_ptr<Packet>> outgoing_packet_queue {}; // packets to send to this peer
};

class NetworkHost : public NonMovable, public NonCopyable {
public:
    NetworkHost(HostType type, uint32_t max_peers = 32, uint32_t max_channels = 2);
    NetworkHost(HostType type, const NetworkAddress &network_address, uint32_t max_peers = 32, uint32_t max_channels = 2);
    ~NetworkHost() = default;

    // Returns nullopt iff there are no available hosts left for connection.
    // Note: Merely makes an attempt at connection and doesn't guarantee its success
    // by itself.
    std::optional<PeerId> Connect(const NetworkAddress &address, double max_wait_time = 1000.0);
    void Disconnect(PeerId peer, double max_wait_time = 1000.0);

    void SendPacket(const NetworkAddress &address, std::unique_ptr<Packet> packet);
    void SendPacket(PeerId peer, ChannelId channel, std::unique_ptr<Packet> packet);

    bool IsConnected(PeerId peer_id) { return m_peers[peer_id].status == PeerStatus::Connected; }

    void Update(double delta_time);

    bool PollNetworkCommand(NetworkCommand &command);
private:
    const double ConnectAttemptRate    = 5.0;         // Hz
    const double ConnectAttemptTime    = 1000.0 / ConnectAttemptRate;

    const double DisconnectAttemptRate = 5.0;         // Hz
    const double DisconnectAttemptTime = 1000.0 / DisconnectAttemptRate;
private:
    std::optional<NetworkAddress> m_address = std::nullopt;
    std::unique_ptr<Socket> m_socket;
    
    const uint32_t m_max_channels;
    const uint32_t m_max_peers;
    uint32_t m_active_peer_count = 0;
    std::vector<NetworkPeer> m_peers;
    HostType m_type;

    // Per peer data:
    uint16_t m_current_sequence = 0;

    std::queue<std::pair<NetworkAddress, std::unique_ptr<Packet>>> m_outgoing_packet_queue {}; // usually for packets that cannot be associated with a peer
private:
    void ValidatePeer(PeerId peer_id) const;
    void FlushPackets(double delta_time);

    void ReceivePackets();

    void HandleConnectionPackets();
    void SendRawPacket(const NetworkAddress &address, const Packet &packet);

    void ResetPeer(NetworkPeer &peer);

    std::optional<PeerId> FindSlot(const NetworkAddress &address);
    std::optional<PeerId> FindEmptySlot();


    // For unreliable sending of large data
    std::vector<std::unique_ptr<Packet>> SplitIntoFragments(const Packet &packet);

    // For reliable sending of large data
    std::vector<std::unique_ptr<Packet>> SplitIntoSlices(uint16_t chunk_id, const Packet &packet);
};

#include "NetworkHost.h"
#include "Packet.h"
#include "Packets/ServerPackets.h"
#include "Packets/SystemPackets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>

#include "../Utils/Math.h"

NetworkHost::NetworkHost(HostType type, uint32_t max_peers, uint32_t max_channels)
    : m_max_peers(max_peers)
    , m_type(type)
    , m_max_channels(max_channels)
{
    m_socket = std::make_unique<Socket>(AddressFamily::IPv4);
    m_socket->MakeNonBlocking();

    // m_peers.reserve(max_peers);
    for (uint32_t i = 0; i < max_peers; ++i) {
        m_peers.emplace_back(static_cast<uint8_t>(max_channels));
        m_peers.back().peer_id = i;
    }

    Assert(max_peers > 0, "Must have positive number of peers!");
}

NetworkHost::NetworkHost(HostType type, const NetworkAddress &address, uint32_t max_peers, uint32_t max_channels)
    : NetworkHost(type, max_peers, max_channels)
{
    m_address = address;
    if (type == HostType::Server) {
        m_socket->Bind(address);
    }
}

std::optional<PeerId> NetworkHost::Connect(const NetworkAddress &address, double max_wait_time) {
    if (FindSlot(address))
        throw std::runtime_error("Cannot connect to host: Host already connected.");

    if (std::optional<PeerId> slot = FindEmptySlot()) {
        NetworkPeer &peer = m_peers[*slot];
        peer.peer_id = *slot;
        peer.status = PeerStatus::Connecting;
        peer.address = address;
        peer.timers = {};
        peer.max_connect_wait_time = max_wait_time;
        m_active_peer_count++;

        // SendPacket<ConnectionRequestPacket>(peer.peer_id);
        ConnectionRequestPacket packet {};
        NetworkBuffer buffer = packet.Serialize();
        m_socket->SendTo(address, buffer);
        
        return slot;
    }
    return std::nullopt;
}

void NetworkHost::Disconnect(PeerId peer_id, double max_wait_time) {
    Assert(peer_id < m_max_peers, std::format("Disconnect: Invalid peer ID {}. The maximum number of peers is {}.", peer_id, m_max_peers));
    NetworkPeer &peer = m_peers[peer_id];
    if (peer.status != PeerStatus::Connected)
        return;

    peer.max_disconnect_wait_time = max_wait_time;
    peer.timers = {};

    DisconnectRequestPacket packet{};
    NetworkBuffer buffer = packet.Serialize();
    m_socket->SendTo(peer.address, buffer);

    if (peer.status == PeerStatus::Disconnected || peer.status == PeerStatus::Disconnecting)
        std::cerr << "Warning: Tried to disconnect Peer " << peer_id << ", which is already disconnected!\n";
    if (peer.status == PeerStatus::Connecting)
        std::cerr << "Warning: Tried to disconnect Peer " << peer_id << ", which is not connected!\n";
}

void NetworkHost::SendPacket(const NetworkAddress &address, std::unique_ptr<Packet> packet) {
    Assert(packet->GetSize() <= MAX_NETWORK_TRANSMISSION_SIZE, "Cannot send large packets to unknown address!");
    m_outgoing_packet_queue.push(std::make_pair(address, std::move(packet)));
}

void NetworkHost::SendPacket(PeerId peer_id, ChannelId channel, std::unique_ptr<Packet> packet) {
    ValidatePeer(peer_id);
    NetworkPeer &peer = m_peers[peer_id];
    packet->channel_id = channel;
    peer.outgoing_packet_queue.push(std::move(packet));

    // Handle splitting logic as necessary
}

void NetworkHost::Update(double delta_time) {
    ReceivePackets();
    FlushPackets(delta_time);
}
    

bool NetworkHost::PollNetworkCommand(NetworkCommand &command) {
    if (m_active_peer_count == 0)
        return false;

    for (PeerId peer_id = 0; peer_id < m_max_peers; ++peer_id) {
        NetworkPeer &peer = m_peers[peer_id];
        if (peer.peer_id != peer_id)
            continue;
        if (peer.status == PeerStatus::Disconnected)
            continue;

        for (ChannelId channel_id = 0; channel_id < m_max_channels; ++channel_id) {
            NetworkChannel &channel = peer.channels[channel_id];
    
            if (channel.incoming_command_queue.empty())
                continue;
    
            command = std::move(channel.incoming_command_queue.front());
            channel.incoming_command_queue.pop();
    
            if (command.type == NetworkCommandType::Disconnect) {
                ResetPeer(peer);
                m_active_peer_count--;
            }
    
            return true;
        }
    }

    return false;
}

void NetworkHost::ValidatePeer(PeerId peer_id) const {
    Assert(peer_id < m_max_peers, std::format("Invalid peer ID {}. The maximum number of peers is {}.", peer_id, m_max_peers));
}

void NetworkHost::ReceivePackets() {
    if (m_type == HostType::Client && m_active_peer_count == 0)
        return;

    // TODO: Throttle how many packets we can receive
    uint32_t num_received = 0;
    while (true) {
        NetworkAddress from {};
        NetworkBuffer data {};

        SocketError error = m_socket->ReceiveFrom(from, data);
        if (error == SocketError::WouldBlock)
            break;

        num_received++;
        std::unique_ptr<Packet> packet = Packet::Deserialize(data);
        ChannelId channel_id = packet->channel_id;
        Assert(channel_id < m_max_channels, "Packet Received: Invalid channel ID!");

        if (!packet)
            continue;

        std::optional<PeerId> peer_id = FindSlot(from);

        //// Disconnect Request Packet ////
        // It is possible that we receive a request from a client we already marked as
        // Disconnected, so we do the decency of sending requests back to the client,
        // so it may disconnect itself.
        if (packet->Type() == PacketType::DisconnectRequest) {
            m_outgoing_packet_queue.push(std::make_pair(from, std::make_unique<DisconnectResponsePacket>()));

            if (peer_id && m_peers[*peer_id].status == PeerStatus::Connected) {
                NetworkChannel &channel = m_peers[*peer_id].channels[channel_id];
                
                NetworkCommand command {};
                command.type = NetworkCommandType::Disconnect;
                command.peer = *peer_id;
                channel.incoming_command_queue.push(std::move(command));
            }
            continue;
        }

        //// Connect Request Packet ////
        if (packet->Type() == PacketType::ConnectionRequest) {
            std::unique_ptr<ConnectionResponsePacket> response = std::make_unique<ConnectionResponsePacket>();
            response->connection_accepted = false;

            // Try to create a new peer if this isn't an existing peer
            if (!peer_id) {
                peer_id = FindEmptySlot();
                if (!peer_id) {
                    std::cout << "Too many clients!\n";
                    m_outgoing_packet_queue.push(std::make_pair(from, std::move(response)));
                    continue;
                } else {
                    // TODO: Send back a challenge instead of blindly accepting such clients
                    m_peers[*peer_id].peer_id = *peer_id;
                    m_peers[*peer_id].address = from;
                    m_peers[*peer_id].status = PeerStatus::Connected;
                    m_active_peer_count++;

                    NetworkChannel &channel = m_peers[*peer_id].channels[channel_id];

                    NetworkCommand command {};
                    command.type = NetworkCommandType::Connect;
                    command.peer = *peer_id;
                    channel.incoming_command_queue.push(std::move(command));
                }
            }

            response->connection_accepted = true;
            SendPacket(*peer_id, 0, std::move(response));
            continue;
        }

        if (!peer_id)
            continue;
        
        NetworkPeer &peer = m_peers[*peer_id];
        NetworkChannel &channel = peer.channels[channel_id];

        //// Disconnect Response Packet ////
        // If peer is disconnecting, complete it if we get a response
        if (peer.status == PeerStatus::Disconnecting && packet->Type() == PacketType::DisconnectResponse) {
            ResetPeer(peer);
            m_active_peer_count--;
            continue;
        }

        //// Connect Response Packet ////
        // If peer is connecting, we see if we got a successful response
        if (peer.status == PeerStatus::Connecting && packet->Type() == PacketType::ConnectionResponse) {
            auto *connection_response = dynamic_cast<ConnectionResponsePacket *>(packet.get());
            peer.status = connection_response->connection_accepted
                ? PeerStatus::Connected
                : PeerStatus::Disconnected;
            if (peer.status == PeerStatus::Disconnected) {
                m_active_peer_count--;
                std::cout << "Could not connect to server!\n";
            } else {
                std::cout << "Successfully connected to server!\n";
            }
            continue;
        }

        if (peer.status != PeerStatus::Connected)
            continue;

        //// Fragment Packet ////
        //// Slice Packet ////
        //// Ack Packet ////

        //// Other Packets ////
        NetworkCommand command {};
        command.type = NetworkCommandType::ReceivePacket;
        command.peer = *peer_id;
        command.packet = std::move(packet);
        channel.incoming_command_queue.push(std::move(command));
    }
}

void NetworkHost::FlushPackets(double delta_time) {    
    while (!m_outgoing_packet_queue.empty()) {
        auto [address, packet] = std::move(m_outgoing_packet_queue.front());
        m_outgoing_packet_queue.pop();

        NetworkBuffer buffer = packet->Serialize();
        m_socket->SendTo(address, buffer);
    }

    // Use network bandwidth based on packet priority;
    for (uint32_t peer_id = 0; peer_id < m_max_peers; ++peer_id) {
        NetworkPeer &peer = m_peers[peer_id];
        if (peer.status == PeerStatus::Disconnected)
            continue;

        uint32_t packet_byte_budget = KbpsToBytes(peer.max_packet_send_bandwidth, delta_time); // bytes
        uint32_t packet_bytes = 0;
        while (packet_bytes <= packet_byte_budget && !peer.outgoing_packet_queue.empty()) {
            if (packet_bytes + peer.outgoing_packet_queue.front()->GetSize() > packet_byte_budget)
                break;

            std::unique_ptr<Packet> packet = std::move(peer.outgoing_packet_queue.front());
            peer.outgoing_packet_queue.pop();
    
            packet_bytes += packet->GetSize();
            Assert(packet_bytes <= packet_byte_budget, std::format("Went over budget for Peer {}!", peer_id));
            Assert(packet->GetSize() <= MAX_NETWORK_TRANSMISSION_SIZE, "Packet too large to send!");

            m_socket->SendTo(peer.address, packet->Serialize());
        }
    }
}

std::optional<PeerId> NetworkHost::FindSlot(const NetworkAddress &address) {
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [address](const NetworkPeer &peer) {
        return address == peer.address && peer.status != PeerStatus::Disconnected;
    });

    if (it == m_peers.end())
        return std::nullopt;
    return static_cast<int>(std::distance(m_peers.begin(), it));
}

std::optional<PeerId> NetworkHost::FindEmptySlot() {
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [](const NetworkPeer &peer) {
        return peer.status == PeerStatus::Disconnected;
    });

    if (it == m_peers.end())
        return std::nullopt;
    return static_cast<int>(std::distance(m_peers.begin(), it));
}

void NetworkHost::ResetPeer(NetworkPeer &peer) {
    peer = NetworkPeer(m_max_channels);
}

std::vector<std::unique_ptr<Packet>> NetworkHost::SplitIntoFragments(const Packet &packet) {
    NetworkBuffer buffer = packet.Serialize();
    
    buffer.Skip(4); // Skip checksum
    uint16_t sequence = buffer.ReadShort();

    int remaining_packet_bytes = buffer.GetSize() - PACKET_DATA_BEGIN - PACKET_DATA_END;
    uint8_t fragment_count = std::floor(remaining_packet_bytes / MAX_PACKET_FRAGMENT_SIZE);
    if (buffer.RemainingBytes() % MAX_PACKET_FRAGMENT_SIZE != 0)
        fragment_count++;

    std::vector<std::unique_ptr<Packet>> result {};
    uint8_t index = 0;
    for (; remaining_packet_bytes > 0; remaining_packet_bytes -= MAX_PACKET_FRAGMENT_SIZE) {
        auto data = buffer.ReadBytes<MAX_PACKET_FRAGMENT_SIZE>(std::min<int>(remaining_packet_bytes, MAX_PACKET_FRAGMENT_SIZE));

        auto fragment_packet = std::unique_ptr<FragmentPacket>();
        fragment_packet->sequence = sequence;
        fragment_packet->fragment_id = index++;
        fragment_packet->num_fragments = fragment_count;
        fragment_packet->fragment_bytes = static_cast<uint16_t>(remaining_packet_bytes);
        fragment_packet->data = std::move(data);

        result.push_back(std::move(fragment_packet));
    }
    return result;
}

std::vector<std::unique_ptr<Packet>> NetworkHost::SplitIntoSlices(uint16_t chunk_id, const Packet &packet) {
    NetworkBuffer buffer = packet.Serialize();

    buffer.Skip(4); // Skip checksum
    uint16_t sequence = buffer.ReadShort();

    int remaining_packet_bytes = buffer.GetSize() - PACKET_DATA_BEGIN - PACKET_DATA_END;
    uint8_t slice_count = std::floor(remaining_packet_bytes / MAX_PACKET_SLICE_SIZE);
    if (buffer.RemainingBytes() % MAX_PACKET_SLICE_SIZE != 0)
        slice_count++;

    std::vector<std::unique_ptr<Packet>> result {};
    uint8_t index = 0;
    for (; remaining_packet_bytes > 0; remaining_packet_bytes -= MAX_PACKET_SLICE_SIZE) {
        auto data = buffer.ReadBytes<MAX_PACKET_SLICE_SIZE>(std::min<int>(remaining_packet_bytes, MAX_PACKET_SLICE_SIZE));

        auto slice_packet = std::unique_ptr<SlicePacket>();
        slice_packet->chunk_id = chunk_id;
        slice_packet->sequence = sequence;
        slice_packet->slice_id = index++;
        slice_packet->num_slices = slice_count;
        slice_packet->slice_bytes = static_cast<uint16_t>(remaining_packet_bytes);
        slice_packet->data = std::move(data);

        result.push_back(std::move(slice_packet));
    }
    return result;
}

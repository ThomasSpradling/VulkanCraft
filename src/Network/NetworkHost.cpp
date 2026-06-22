#include "NetworkHost.h"
#include "Packets.h"
#include "Packets/ServerPackets.h"
#include "Packets/SystemPackets.h"
#include <algorithm>
#include <array>
#include <iterator>
#include <stdexcept>

NetworkHost::NetworkHost(HostType type, uint32_t max_peers)
    : m_max_peers(max_peers)
    , m_type(type)
{
    m_socket = std::make_unique<Socket>(AddressFamily::IPv4);

    if (type == HostType::Client)
        max_peers = 1;
    m_peers.resize(max_peers);

    Assert(max_peers > 0, "Must have positive number of peers!");
}

NetworkHost::NetworkHost(HostType type, const NetworkAddress &network_address, uint32_t max_peers)
    : NetworkHost(type, max_peers)
{
    m_address = network_address;
    m_socket->Bind(network_address);
}

std::optional<Packet> NetworkHost::ReceivePacket(NetworkPeer &peer) {
    NetworkBuffer buffer {};
    NetworkAddress from {};
    if (m_socket->ReceiveFrom(buffer, from) == SocketError::WouldBlock) {
        return std::nullopt;
    }

    Packet packet{};
    packet.Read(buffer);

    std::optional<uint32_t> slot = GetAddressSlot(from);
    if (packet.packet_type == PacketType::ConnectionRequest && m_type == HostType::Server) {
        PacketConnectionResponse response {
            .connection_accepted = false,
        };

        if (slot == std::nullopt) {
            // If this address is not found in any slot, try to place this client there
            if (auto empty_slot = FindEmptySlot(); empty_slot != std::nullopt) {
                std::cout << "Accepted Client " << *empty_slot << "\n";
                response.connection_accepted = true;
                m_peers[*empty_slot] = {
                    .address = from,
                    .connected = ConnectState::Connected,
                };
            }
        } else {
            response.connection_accepted = true;
        }

        SendRawPacket(from, Packet{
            .packet_type = PacketType::ConnectionResponse,
            .packet_data = response,
        });
    }

    if (packet.packet_type == PacketType::ConnectionResponse && m_type == HostType::Client) {
        auto response = std::get<PacketConnectionResponse>(packet.packet_data);
        if (response.connection_accepted) {
            std::cout << "Successfully connected to a server!\n";
            m_peers[0].connected = ConnectState::Connected;
        } else {
            std::cout << "Could not connect to server!\n";
            m_peers[0].connected = ConnectState::Disconnected;
        }
    }

    if (packet.packet_type == PacketType::DisconnectRequest && m_type == HostType::Server) {
        if (slot != std::nullopt) {
            std::cout << "Client " << *slot << " disconnected!\n";
            m_peers[*slot] = {};
        }

        // Allow send to any address so that a disconnected client eventually actually receives
        // the response
        SendRawPacket(from, Packet{
            .packet_type = PacketType::DisconnectResponse,
        });
    }

    if (packet.packet_type == PacketType::DisconnectResponse && m_type == HostType::Client) {
        std::cout << "Successfull disconnected!\n";
        m_peers[0] = {};
    }

    if (slot != std::nullopt) {
        NetworkPeer copy = m_peers[*slot];
        peer = copy;
    }

    return packet;
}

void NetworkHost::SendPacket(const NetworkPeer &peer, const Packet &packet, bool reliable) {
    // TODO: instead place in packet queue
    SendRawPacket(peer.address, packet);
    // Packet pack = packet;
    // pack.sequence = m_current_sequence++;

    // if (reliable) {
    //     std::vector<Packet> slices = SplitIntoSlices(0, pack);
    // } else {
    //     std::vector<Packet> fragments = SplitIntoSlices(0, pack);
    // }
}

std::optional<NetworkPeer> NetworkHost::ConnectToHost(const NetworkAddress &address) {
    if (m_type == HostType::Server)
        throw std::runtime_error("Server cannot connect to a separate host!\n");

    // send connection request to host
    SendRawPacket(address, Packet{
        .packet_type = PacketType::ConnectionRequest,
        .sequence = 0,
    });
    m_peers[0].connected = ConnectState::Connecting;
    return m_peers[0];
}

void NetworkHost::DisconnectFromHost(const NetworkAddress &address) {
    if (m_type == HostType::Server)
        throw std::runtime_error("Server cannot disconnect from a separate host!\n");

    // send connection request to host
    SendRawPacket(address, Packet{
        .packet_type = PacketType::DisconnectRequest
    });
}

void NetworkHost::HandleConnectionPackets() {

}

void NetworkHost::SendRawPacket(const NetworkAddress &address, const Packet &packet) {
    NetworkBuffer buffer{};
    packet.Write(buffer);
    m_socket->SendTo(buffer, address);
}

std::optional<uint32_t> NetworkHost::GetAddressSlot(const NetworkAddress &address) {
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [address](const NetworkPeer &peer) {
        return address == peer.address;
    });

    if (it == m_peers.end())
        return std::nullopt;
    return static_cast<int>(std::distance(m_peers.begin(), it));
}

std::optional<uint32_t> NetworkHost::FindEmptySlot() {
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [](const NetworkPeer &peer) {
        return peer.connected == ConnectState::Disconnected;
    });

    if (it == m_peers.end())
        return std::nullopt;
    return static_cast<int>(std::distance(m_peers.begin(), it));
}

std::vector<Packet> NetworkHost::SplitIntoFragments(const Packet &packet) {
    NetworkBuffer buffer {};
    packet.Write(buffer);

    if (buffer.GetSize() < MAX_PACKET_FRAGMENT_SIZE + PACKET_DATA_BEGIN + PACKET_DATA_END) {
        return { packet };
    }
    
    buffer.Skip(4); // Skip checksum
    uint16_t sequence = buffer.ReadShort();

    int remaining_packet_bytes = buffer.GetSize() - PACKET_DATA_BEGIN - PACKET_DATA_END;
    uint8_t fragment_count = std::floor(remaining_packet_bytes / MAX_PACKET_FRAGMENT_SIZE);
    if (buffer.RemainingBytes() % MAX_PACKET_FRAGMENT_SIZE != 0)
        fragment_count++;

    std::vector<Packet> result {};
    uint8_t index = 0;
    for (; remaining_packet_bytes > 0; remaining_packet_bytes -= MAX_PACKET_FRAGMENT_SIZE) {
        auto data = buffer.ReadBytes<MAX_PACKET_FRAGMENT_SIZE>(std::min<int>(remaining_packet_bytes, MAX_PACKET_FRAGMENT_SIZE));

        result.push_back(Packet{
            .packet_type = PacketType::Fragment,
            .sequence = sequence,
            .packet_data = PacketFragment{
                .fragment_id = index++,
                .num_fragments = fragment_count,
                .fragment_bytes = static_cast<uint16_t>(remaining_packet_bytes),
                .data = std::move(data),
            },
        });
    }
    return result;
}

std::vector<Packet> NetworkHost::SplitIntoSlices(uint16_t chunk_id, const Packet &packet) {
    NetworkBuffer buffer {};
    packet.Write(buffer);

    if (buffer.GetSize() < MAX_PACKET_SLICE_SIZE + PACKET_DATA_BEGIN + PACKET_DATA_END) {
        return { packet };
    }
    
    buffer.Skip(4); // Skip checksum
    uint16_t sequence = buffer.ReadShort();

    int remaining_packet_bytes = buffer.GetSize() - PACKET_DATA_BEGIN - PACKET_DATA_END;
    uint8_t slice_count = std::floor(remaining_packet_bytes / MAX_PACKET_SLICE_SIZE);
    if (buffer.RemainingBytes() % MAX_PACKET_SLICE_SIZE != 0)
        slice_count++;

    std::vector<Packet> result {};
    uint8_t index = 0;
    for (; remaining_packet_bytes > 0; remaining_packet_bytes -= MAX_PACKET_SLICE_SIZE) {
        auto data = buffer.ReadBytes<MAX_PACKET_SLICE_SIZE>(std::min<int>(remaining_packet_bytes, MAX_PACKET_SLICE_SIZE));

        result.push_back(Packet{
            .packet_type = PacketType::Slice,
            .sequence = sequence,
            .packet_data = PacketSlice{
                .chunk_id = chunk_id,
                .slice_id = index++,
                .num_slices = slice_count,
                .slice_bytes = static_cast<uint16_t>(remaining_packet_bytes),
                .data = std::move(data),
            },
        });
    }
    return result;
}

#include "PacketUtils.h"
#include "Common.h"
#include <stdarg.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Radis
{
    // Encode a NetMessage into an ENetPacket
    ENetPacket* PacketUtils::encode(const NetMessage& msg) const
    {
        std::vector<std::uint8_t> payload = nlohmann::json::to_msgpack(msg.body);

        PacketHeader header{};
        header.packetId = static_cast<std::uint16_t>(msg.id);
        header.flags = 0;

        const std::size_t totalSize = sizeof(PacketHeader) + payload.size();
        std::vector<std::uint8_t> buffer(totalSize);

        std::memcpy(buffer.data(), &header, sizeof(PacketHeader));

        if (!payload.empty())
        {
            std::memcpy(buffer.data() + sizeof(PacketHeader), payload.data(), payload.size());
        }

        return enet_packet_create(buffer.data(), buffer.size(), ENET_PACKET_FLAG_RELIABLE);
    }

    // Decode an ENetPacket into a NetMessage
    bool PacketUtils::decode(const ENetPacket* packet, NetMessage& outMsg) const
    {
        if (!packet || packet->dataLength < sizeof(PacketHeader))
        {
            std::printf("Packet too small for header.\n");
            return false;
        }

        PacketHeader header{};
        std::memcpy(&header, packet->data, sizeof(PacketHeader));

        const auto* payloadBegin = reinterpret_cast<const std::uint8_t*>(packet->data) + sizeof(PacketHeader);
        const auto* payloadEnd = reinterpret_cast<const std::uint8_t*>(packet->data) + packet->dataLength;

        try
        {
            outMsg.body = nlohmann::json::from_msgpack(payloadBegin, payloadEnd);
        }
        catch (const std::exception& e)
        {
            std::printf("Failed to decode msgpack: %s\n", e.what());
            return false;
        }

        outMsg.id = static_cast<PacketID>(header.packetId);
        return true;
    }

    void PacketUtils::send(ENetPeer* peer, const NetMessage& msg) const
    {
        if (!peer) return;
        ENetPacket* pkt = encode(msg);
        enet_peer_send(peer, 0, pkt);
    }

    void PacketUtils::send(ENetPeer* peer, PacketID id, const nlohmann::json& body) const
    {
        if (!peer) return;
        NetMessage msg{ id, body };
        ENetPacket* pkt = encode(msg);
        enet_peer_send(peer, 0, pkt);
    }

    void PacketUtils::sendWithBinary(ENetPeer* peer, PacketID id, nlohmann::json meta, const std::vector<std::uint8_t>& blob, std::string_view fieldName) const
    {
        if (!peer) return;

        meta[std::string(fieldName)] = nlohmann::json::binary(blob);
        NetMessage msg{ id, std::move(meta) };

        ENetPacket* pkt = encode(msg);
        enet_peer_send(peer, 0, pkt);
    }

    void PacketUtils::broadcast(ENetHost* host, ENetPeer* sender, const NetMessage& msg) const
    {
        ForEachOtherPeer(host, sender, [this, &msg](ENetPeer* p) 
        {
            send(p, msg);
        });
    }

    void PacketUtils::broadcast(ENetHost* host, ENetPeer* sender, PacketID id, const nlohmann::json& body) const
    {
        NetMessage msg{ id, body };
        broadcast(host, sender, msg);
    }

} // namespace Radis

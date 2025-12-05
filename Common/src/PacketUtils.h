#pragma once
#include "enet/enet.h"
#include "Common.h"

namespace Radis
{
    struct PacketHeader
    {
        std::uint16_t packetId; // Radis::PacketID
        std::uint16_t flags;    // reserved (compression, etc.)
    };

    struct NetMessage
    {
        PacketID id;
        nlohmann::json body;

        NetMessage(PacketID pid = PacketID::INIT_PLAYER_PACKET, nlohmann::json j = nlohmann::json::object()) 
            : id(pid), body(std::move(j)) 
        {
        }
    };

    class PacketUtils
    {
    public:
        PacketUtils() = default;
        ~PacketUtils() = default;

        // Encode / Decode
        ENetPacket* encode(const NetMessage& msg) const;
        bool        decode(const ENetPacket* packet, NetMessage& outMsg) const;

        // Sending
        void send(ENetPeer* peer, const NetMessage& msg) const;

        // convenience: pass id + json directly
        void send(ENetPeer* peer, PacketID  id, const nlohmann::json& body = nlohmann::json::object()) const;

        // json + binary blob (stored as msgpack "bin" under fieldName)
        void sendWithBinary(ENetPeer* peer, PacketID  id, nlohmann::json meta, const std::vector<std::uint8_t>& blob, std::string_view fieldName = "blob") const;

        // Broadcast
        void broadcast(ENetHost* host, ENetPeer* sender, const NetMessage& msg) const;
        void broadcast(ENetHost* host, ENetPeer* sender, PacketID  id, const nlohmann::json& body = nlohmann::json::object()) const;

    private:
        template<typename Fn>
        void ForEachOtherPeer(ENetHost* host, ENetPeer* sender, Fn&& fn) const
        {
            if (!host) return;
            for (size_t i = 0; i < host->peerCount; ++i)
            {
                ENetPeer* p = &host->peers[i];
                if (p->state != ENET_PEER_STATE_CONNECTED || p == sender) continue;
                fn(p);
            }
        }
    };
}

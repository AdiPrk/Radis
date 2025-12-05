#pragma once

namespace Radis
{
    // Forward declaration
    class PlayerManager;

    class PacketHandler {
    public:
        PacketHandler(PacketUtils& utils);
        ~PacketHandler();

        // --- Packet handling ---
        void HandlePacket(ENetPeer* peer, ENetPacket* packet, ENetHost* host, PlayerManager& playerManager);

    private:
        PacketUtils& packetUtils;
    };

}
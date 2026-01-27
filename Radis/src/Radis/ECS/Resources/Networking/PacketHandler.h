/*****************************************************************//**
 * \file   PacketHandler.h
 * \brief  Handles incoming network packets
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
{
    // Forward Declaration
    class PlayerManager;

    class PacketHandler {
    public:
        PacketHandler(PacketUtils& utils);
        ~PacketHandler();

        // --- Packet handling ---
        void HandlePacket(ENetPeer* peer, ENetPacket* packet, PlayerManager& playerManager);

    private:
        PacketUtils& packetUtils;
    };

}